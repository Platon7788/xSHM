#ifndef SHM_IPC_H
#define SHM_IPC_H

#include "shm_types.h"
#include "shm_ringbuffer.h"
#include "shm_events.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Server structure
    struct shm_server_t {
        shm_ring_t* tx_ring;           // Server -> Client ring buffer
        shm_ring_t* rx_ring;           // Client -> Server ring buffer
        shm_event_ctx_t* event_ctx;
        char name[256];
        bool running;
        // --- СОСТОЯНИЕ ПОДКЛЮЧЕНИЯ (определяется автоматически через active_readers) ---
        volatile bool client_connected;      // Флаг подключения клиента
        volatile uint32_t last_known_readers; // Последнее известное количество читателей
        // ------------------------------------
    };

    // Client structure
    struct shm_client_t {
        shm_ring_t* tx_ring;           // Client -> Server ring buffer
        shm_ring_t* rx_ring;           // Server -> Client ring buffer
        shm_event_ctx_t* event_ctx;
        char name[256];
        bool connected;
    };

    // ============================================================================
    // SERVER API
    // ============================================================================

    // Create server
    SHM_API shm_server_t* shm_server_create(const char* name, const shm_ring_config_t* config);

    // Destroy server
    SHM_API void shm_server_destroy(shm_server_t* server);

    // Send data from server to client (проверяет флаг client_connected)
    SHM_API shm_error_t shm_server_send(shm_server_t* server, const void* data, uint32_t size);

    // Receive data from client
    SHM_API shm_error_t shm_server_receive(shm_server_t* server, void* data, uint32_t* size);

    SHM_API bool shm_server_check_connection_status(shm_server_t* server);  // Публичная проверка статуса подключения

    // Register server event callback
    SHM_API shm_error_t shm_server_register_callback(shm_server_t* server,
        shm_event_callback_t callback,
        void* user_data);

    // ============================================================================
    // CLIENT API
    // ============================================================================

    // Connect client to server (подключение определяется сервером автоматически)
    SHM_API shm_client_t* shm_client_connect(const char* name);

    // Disconnect client
    SHM_API void shm_client_disconnect(shm_client_t* client);

    // Send data from client to server
    SHM_API shm_error_t shm_client_send(shm_client_t* client, const void* data, uint32_t size);

    // Receive data from server
    SHM_API shm_error_t shm_client_receive(shm_client_t* client, void* data, uint32_t* size);

    // Register client event callback
    SHM_API shm_error_t shm_client_register_callback(shm_client_t* client,
        shm_event_callback_t callback,
        void* user_data);

    // ============================================================================
    // CONVENIENCE MACROS
    // ============================================================================

    // Default configuration (2 x 4MB rings = 8MB total)
#define SHM_DEFAULT_CONFIG() \
    (shm_ring_config_t){ \
        .size = 4 * 1024 * 1024,      /* 4 MB per ring */ \
        .max_readers = 4, \
        .blocking = true, \
        .timeout_ms = 5000 \
    }

// Create server with default config
#define SHM_CREATE_SERVER(name) \
    ({ \
        shm_ring_config_t cfg = SHM_DEFAULT_CONFIG(); \
        shm_server_create(name, &cfg); \
    })

// Connect client
#define SHM_CONNECT_CLIENT(name) \
    shm_client_connect(name)

// Server send macro
#define SHM_SERVER_SEND(server, data, size) \
    shm_server_send(server, data, size)

// Server receive macro
#define SHM_SERVER_RECEIVE(server, buffer, size_ptr) \
    shm_server_receive(server, buffer, size_ptr)

// Client send macro
#define SHM_CLIENT_SEND(client, data, size) \
    shm_client_send(client, data, size)

// Client receive macro
#define SHM_CLIENT_RECEIVE(client, buffer, size_ptr) \
    shm_client_receive(client, buffer, size_ptr)

// Register server callback with auto-subscribe
#define SHM_SERVER_ON_EVENT(server, callback_func, user_data) \
    ({ \
        shm_error_t err = shm_server_register_callback(server, callback_func, user_data); \
        if (err == SHM_SUCCESS) { \
            shm_event_start_listener(server->event_ctx); \
        } \
        err; \
    })

// Register client callback with auto-subscribe
#define SHM_CLIENT_ON_EVENT(client, callback_func, user_data) \
    ({ \
        shm_error_t err = shm_client_register_callback(client, callback_func, user_data); \
        if (err == SHM_SUCCESS) { \
            shm_event_start_listener(client->event_ctx); \
        } \
        err; \
    })

// Quick server setup macro
#define SHM_SETUP_SERVER(name, callback, userdata) \
    ({ \
        shm_server_t* srv = SHM_CREATE_SERVER(name); \
        if (srv) { \
            SHM_SERVER_ON_EVENT(srv, callback, userdata); \
        } \
        srv; \
    })

// Quick client setup macro
#define SHM_SETUP_CLIENT(name, callback, userdata) \
    ({ \
        shm_client_t* cli = SHM_CONNECT_CLIENT(name); \
        if (cli) { \
            SHM_CLIENT_ON_EVENT(cli, callback, userdata); \
        } \
        cli; \
    })

// Helper macro for sending structs
#define SHM_SEND_STRUCT(endpoint, structptr) \
    _Generic((endpoint), \
        shm_server_t*: shm_server_send, \
        shm_client_t*: shm_client_send \
    )(endpoint, structptr, sizeof(*(structptr)))

// Helper macro for receiving structs
#define SHM_RECV_STRUCT(endpoint, structptr) \
    ({ \
        uint32_t sz = sizeof(*(structptr)); \
        _Generic((endpoint), \
            shm_server_t*: shm_server_receive, \
            shm_client_t*: shm_client_receive \
        )(endpoint, structptr, &sz); \
    })

#ifdef __cplusplus
}
#endif

#endif // SHM_IPC_H