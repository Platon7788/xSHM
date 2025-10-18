#include "../include/shm_ipc.h"
#include <strsafe.h>  // Для strncpy_s и _TRUNCATE
#include <stdio.h>
#include <string.h>
#include <windows.h> // Для Sleep

// --- УДАЛЕНО: SHM_CLIENT_HELLO_MESSAGE ---

shm_server_t* shm_server_create(const char* name, const shm_ring_config_t* config) {
    if (!name || !config) {
        return NULL;
    }

    shm_server_t* server = (shm_server_t*)calloc(1, sizeof(shm_server_t));
    if (!server) {
        return NULL;
    }

    strncpy_s(server->name, sizeof(server->name), name, _TRUNCATE);

    char tx_name[300];
    char rx_name[300];
    // --- ИСПРАВЛЕНО: Использование sprintf_s для безопасности ---
    errno_t err1 = sprintf_s(tx_name, sizeof(tx_name), "%s_tx", name);
    errno_t err2 = sprintf_s(rx_name, sizeof(rx_name), "%s_rx", name);
    if (err1 < 0 || err2 < 0) {
        free(server);
        return NULL;
    }
    // ---------------------------------------------

    server->tx_ring = shm_ring_create(tx_name, config);
    if (!server->tx_ring) {
        free(server);
        return NULL;
    }

    server->rx_ring = shm_ring_create(rx_name, config);
    if (!server->rx_ring) {
        shm_ring_close(server->tx_ring);
        free(server);
        return NULL;
    }

    server->event_ctx = shm_event_create(name, true);
    if (!server->event_ctx) {
        shm_ring_close(server->tx_ring);
        shm_ring_close(server->rx_ring);
        free(server);
        return NULL;
    }

    server->running = true;
    // --- ИНИЦИАЛИЗАЦИЯ НОВЫХ ПОЛЕЙ ---
    server->client_connected = false;
    server->last_known_readers = 0;
    // ----------------------------------

    return server;
}

void shm_server_destroy(shm_server_t* server) {
    if (!server) {
        return;
    }

    server->running = false;

    if (server->event_ctx) {
        shm_event_signal(server->event_ctx, SHM_EVENT_DISCONNECT);
        shm_event_destroy(server->event_ctx);
    }

    if (server->tx_ring) {
        shm_ring_close(server->tx_ring);
    }

    if (server->rx_ring) {
        shm_ring_close(server->rx_ring);
    }

    free(server);
}

// --- НОВОЕ: Внутренняя функция для проверки статуса подключения ---
// Возвращает true, если статус изменился (подключился/отключился)
bool shm_server_check_connection_status(shm_server_t* server) {
    if (!server) {
        return false;
    }

    bool status_changed = false;
    uint32_t current_readers = server->tx_ring->header->active_readers; // volatile read

    if (current_readers > 0) {
        // Есть читатели
        if (!server->client_connected) {
            // Клиент только что подключился
            server->client_connected = true;
            server->last_known_readers = current_readers; // Обновляем последнее известное значение
            printf("Server: Client connected (detected via active_readers change).\n");
            // --- СИГНАЛИЗИРУЕМ СОБЫТИЕ ПОДКЛЮЧЕНИЯ ---
            if (server->event_ctx) {
                shm_event_signal(server->event_ctx, SHM_EVENT_CONNECT);
            }
            // ----------------------------------------
            status_changed = true;
        }
        else {
            // Клиент был подключен, возможно, количество читателей изменилось (например, появился ещё один, что маловероятно в 1:1)
            // Но для согласованности обновим last_known_readers
            server->last_known_readers = current_readers;
        }
    }
    else {
        // Нет читателей
        if (server->client_connected) {
            // Клиент отключился (ранее был подключен)
            server->client_connected = false;
            server->last_known_readers = 0; // Сбросим для надёжности
            printf("Server: Client disconnected (detected via active_readers drop to 0).\n");
            // --- СИГНАЛИЗИРУЕМ СОБЫТИЕ ОТКЛЮЧЕНИЯ ---
            if (server->event_ctx) {
                shm_event_signal(server->event_ctx, SHM_EVENT_DISCONNECT);
            }
            // ----------------------------------------
            status_changed = true;
        }
        // else: Клиент не был подключен, ничего не изменилось
    }

    return status_changed;
}
// ----------------------------------------------

shm_error_t shm_server_send(shm_server_t* server, const void* data, uint32_t size) {
    if (!server || !data || size == 0) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // --- НОВОЕ: Проверка статуса подключения перед отправкой ---
    shm_server_check_connection_status(server);
    // ------------------------------------------------------------

    // --- ПРОВЕРКА ПОДКЛЮЧЕНИЯ ---
    if (!server->client_connected) {
        // Клиент не подключен, игнорируем отправку
        // Можно логировать, если нужно
        // printf("Server: Client not connected, ignoring send.\n");
        return SHM_SUCCESS; // Или другой код, если нужно сигнализировать об игнорировании
    }
    // ---------------------------------------

    shm_error_t result = shm_ring_write(server->tx_ring, data, size);

    // SHM_ERROR_FULL больше не возвращается из shm_ring_write, так что result всегда SHM_SUCCESS
    // (если data и size валидны и size <= MAX_EMBEDDED_MESSAGE_SIZE)

    // Сигнал событий может быть неактуален в overwriting буфере, но оставим для совместимости
    // и для уведомления о *новых данных*, отправленных *после* подключения
    if (result == SHM_SUCCESS && server->event_ctx) {
        shm_event_signal(server->event_ctx, SHM_EVENT_DATA_AVAILABLE);
    }

    return result;
}

shm_error_t shm_server_receive(shm_server_t* server, void* data, uint32_t* size) {
    if (!server || !data || !size) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // --- НОВОЕ: Проверка статуса подключения перед приёмом ---
    // Это помогает быстрее реагировать на отключение клиента, если он отключился между send и receive
    shm_server_check_connection_status(server);
    // ------------------------------------------------------------

    shm_error_t result = shm_ring_read(server->rx_ring, data, size);

    // --- УДАЛЕНО: Проверка HELLO сообщения ---
    // Вся логика теперь на уровне мониторинга active_readers
    // ------------------------------------------

    return result;
}

shm_error_t shm_server_register_callback(shm_server_t* server,
    shm_event_callback_t callback,
    void* user_data) {
    if (!server || !callback) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // --- НОВОЕ: Устанавливаем owner для периодической проверки ---
    server->event_ctx->owner = server;
    server->event_ctx->is_server = true;  // Явно, на всякий случай
    // ------------------------------------------------------------

    // shm_event_register_callback теперь сам запустит слушатель при установке первого callback
    return shm_event_register_callback(server->event_ctx, callback, user_data);
}

shm_client_t* shm_client_connect(const char* name) {
    if (!name) {
        return NULL;
    }

    shm_client_t* client = (shm_client_t*)calloc(1, sizeof(shm_client_t));
    if (!client) {
        return NULL;
    }

    strncpy_s(client->name, sizeof(client->name), name, _TRUNCATE);

    char tx_name[300];
    char rx_name[300];
    errno_t err1 = sprintf_s(tx_name, sizeof(tx_name), "%s_rx", name); // Клиент пишет в rx_ring сервера
    errno_t err2 = sprintf_s(rx_name, sizeof(rx_name), "%s_tx", name); // Клиент читает из tx_ring сервера
    if (err1 < 0 || err2 < 0) {
        free(client);
        return NULL;
    }


    // Retry loop для открытия tx_ring (ожидание готовности сервера, без фиксированного Sleep)
    int retries = 50;  // ~500 мс всего (10 мс x 50)
    client->tx_ring = NULL;
    while (retries > 0 && !client->tx_ring) {
        client->tx_ring = shm_ring_open(tx_name);
        if (!client->tx_ring) {
            Sleep(10);  // Короткий сон между попытками (низкий CPU)
            retries--;
        }
    }
    if (!client->tx_ring) {
        free(client);
        return NULL;
    }

    // Аналогично для rx_ring
    retries = 50;
    client->rx_ring = NULL;
    while (retries > 0 && !client->rx_ring) {
        client->rx_ring = shm_ring_open(rx_name);
        if (!client->rx_ring) {
            Sleep(10);
            retries--;
        }
    }
    if (!client->rx_ring) {
        shm_ring_close(client->tx_ring);
        free(client);
        return NULL;
    }

    client->event_ctx = shm_event_create(name, false);
    if (!client->event_ctx) {
        shm_ring_close(client->tx_ring);
        shm_ring_close(client->rx_ring);
        free(client);
        return NULL;
    }

    client->connected = true;

    // --- УДАЛЕНО: Отправка HELLO сообщения ---
    // Клиент больше не отправляет специальное сообщение HELLO.
    // Подключение определяется автоматически сервером через active_readers.
    // ------------------------------------------

    return client;
}

void shm_client_disconnect(shm_client_t* client) {
    if (!client) {
        return;
    }

    client->connected = false;

    if (client->event_ctx) {
        shm_event_signal(client->event_ctx, SHM_EVENT_DISCONNECT);
        shm_event_destroy(client->event_ctx);
    }

    if (client->tx_ring) {
        shm_ring_close(client->tx_ring);
    }

    if (client->rx_ring) {
        shm_ring_close(client->rx_ring);
    }

    free(client);
    // --- ПРИМЕЧАНИЕ: При закрытии shm_ring_close, active_readers в соответствующем
    // буфере сервера (tx_ring клиента) автоматически уменьшится. ---
}

shm_error_t shm_client_send(shm_client_t* client, const void* data, uint32_t size) {
    if (!client || !data || size == 0) {
        return SHM_ERROR_INVALID_PARAM;
    }

    shm_error_t result = shm_ring_write(client->tx_ring, data, size);

    // SHM_ERROR_FULL больше не возвращается из shm_ring_write, так что result всегда SHM_SUCCESS
    // (если data и size валидны и size <= MAX_EMBEDDED_MESSAGE_SIZE)

    // Сигнал событий может быть неактуален в overwriting буфере, но оставим для совместимости
    if (result == SHM_SUCCESS && client->event_ctx) {
        shm_event_signal(client->event_ctx, SHM_EVENT_DATA_AVAILABLE);
    }

    return result;
}

shm_error_t shm_client_receive(shm_client_t* client, void* data, uint32_t* size) {
    if (!client || !data || !size) {
        return SHM_ERROR_INVALID_PARAM;
    }

    return shm_ring_read(client->rx_ring, data, size);
}

shm_error_t shm_client_register_callback(shm_client_t* client,
    shm_event_callback_t callback,
    void* user_data) {
    if (!client || !callback) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // --- НОВОЕ: Устанавливаем owner (для клиента не используется, но симметрично) ---
    client->event_ctx->owner = client;
    client->event_ctx->is_server = false;
    // ------------------------------------------------------------

    // shm_event_register_callback теперь сам запустит слушатель при установке первого callback
    return shm_event_register_callback(client->event_ctx, callback, user_data);
}
