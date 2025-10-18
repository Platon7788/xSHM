#include "../include/shm_events.h"
#include "../include/shm_ipc.h"
#include <stdio.h>

// --- ���Ĩ� ������ ��� ������� � ������������ ---
#define SHM_EVENT_COUNT 5
// ----------------------------------------------

static DWORD WINAPI event_listener_thread(LPVOID param) {
    shm_event_ctx_t* ctx = (shm_event_ctx_t*)param;

    while (ctx->running) {
        // ����������� ���������� ��������� �������� �� SHM_EVENT_COUNT
        DWORD result = WaitForMultipleObjects(
            SHM_EVENT_COUNT, // ���� 4, ����� SHM_EVENT_COUNT
            ctx->event_handles, // ������ ������� ������ 5 * sizeof(HANDLE)
            FALSE, // Wait for any object
            ctx->timeout_ms
        );

        if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + SHM_EVENT_COUNT) { // ���� +4, ����� +SHM_EVENT_COUNT
            shm_event_type_t event_type = (shm_event_type_t)(result - WAIT_OBJECT_0);

            if (ctx->callback) {
                ctx->callback(ctx->user_data, event_type, NULL, 0);
            }
            // Reset event implicitly by auto-reset nature
        }
        else if (result == WAIT_TIMEOUT) {
            // --- �����: ������������� �������� ������� ����������� ��� ������� ---
            if (ctx->is_server && ctx->owner && ctx->running) {
                shm_server_check_connection_status((shm_server_t*)ctx->owner);
                // ���� ������ ���������, check ���� ���������� CONNECT/DISCONNECT,
                // ��� �������� ��������� WaitForMultipleObjects � ������� callback
            }
            // ---------------------------------------------------------------
            continue;
        }
        else {
            break; // ������ ��������
        }
    }

    return 0;
}

shm_event_ctx_t* shm_event_create(const char* name, bool is_server) {
    if (!name) {
        return NULL;
    }

    shm_event_ctx_t* ctx = (shm_event_ctx_t*)calloc(1, sizeof(shm_event_ctx_t));
    if (!ctx) {
        return NULL;
    }

    // --- �����: ������������� ����� ����� ---
    ctx->is_server = is_server;
    ctx->owner = NULL;
    // ----------------------------------------

    // ����������� ������ ��� �� SHM_EVENT_COUNT
    char event_names[SHM_EVENT_COUNT][300];
    snprintf(event_names[0], sizeof(event_names[0]), "Local\\SHM_DATA_%s", name);
    snprintf(event_names[1], sizeof(event_names[1]), "Local\\SHM_SPACE_%s", name);
    snprintf(event_names[2], sizeof(event_names[2]), "Local\\SHM_DISCONNECT_%s", name);
    snprintf(event_names[3], sizeof(event_names[3]), "Local\\SHM_ERROR_%s", name);
    snprintf(event_names[4], sizeof(event_names[4]), "Local\\SHM_CONNECT_%s", name); // ����� �������

    // ������/��������� SHM_EVENT_COUNT �������
    for (int i = 0; i < SHM_EVENT_COUNT; i++) {
        if (is_server) {
            ctx->event_handles[i] = CreateEventA(NULL, FALSE, FALSE, event_names[i]);
        }
        else {
            ctx->event_handles[i] = OpenEventA(EVENT_ALL_ACCESS, FALSE, event_names[i]);
        }

        if (!ctx->event_handles[i]) {
            shm_event_destroy(ctx);
            return NULL;
        }
    }

    ctx->callback = NULL;
    ctx->user_data = NULL;
    ctx->worker_thread = NULL;
    ctx->running = false; // ���������� �� �������
    ctx->timeout_ms = 100;

    return ctx;
}

void shm_event_destroy(shm_event_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    shm_event_stop_listener(ctx); // ������������� �����, ���� �� �������

    for (int i = 0; i < SHM_EVENT_COUNT; i++) { // ���������� ������
        if (ctx->event_handles[i]) {
            CloseHandle(ctx->event_handles[i]);
        }
    }

    free(ctx);
}

shm_error_t shm_event_register_callback(shm_event_ctx_t* ctx,
    shm_event_callback_t callback,
    void* user_data) {
    if (!ctx || !callback) {
        return SHM_ERROR_INVALID_PARAM;
    }

    ctx->callback = callback;
    ctx->user_data = user_data;

    // --- �����: ���������� ��������� ��� ����������� ������� callback ---
    // ���������, ������� �� ���������
    if (!ctx->running) {
        shm_error_t start_result = shm_event_start_listener(ctx);
        if (start_result != SHM_SUCCESS) {
            // ���� �� ������� ���������, ������� callback
            ctx->callback = NULL;
            ctx->user_data = NULL;
            return start_result; // ���������� ������ �������
        }
    }
    // ---------------------------------------------------------------

    return SHM_SUCCESS; // ���������� ��������� ������� ���������
}

shm_error_t shm_event_signal(shm_event_ctx_t* ctx, shm_event_type_t event_type) {
    if (!ctx || event_type < 0 || event_type >= SHM_EVENT_COUNT) { // ���������� ������
        return SHM_ERROR_INVALID_PARAM;
    }

    if (!SetEvent(ctx->event_handles[event_type])) { // ������ ��������� ���������� � �������
        return SHM_ERROR_ACCESS;
    }

    return SHM_SUCCESS;
}

shm_error_t shm_event_wait(shm_event_ctx_t* ctx, shm_event_type_t event_type, uint32_t timeout_ms) {
    if (!ctx || event_type < 0 || event_type >= SHM_EVENT_COUNT) { // ���������� ������
        return SHM_ERROR_INVALID_PARAM;
    }

    DWORD result = WaitForSingleObject(ctx->event_handles[event_type], timeout_ms);

    if (result == WAIT_OBJECT_0) {
        return SHM_SUCCESS;
    }
    else if (result == WAIT_TIMEOUT) {
        return SHM_ERROR_TIMEOUT;
    }
    else {
        return SHM_ERROR_ACCESS;
    }
}

shm_error_t shm_event_start_listener(shm_event_ctx_t* ctx) {
    if (!ctx) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // --- ��������: ��������, ������� �� ��� ��������� ---
    if (ctx->running) {
        return SHM_SUCCESS; // ��� �������, ���������� �����
    }
    // ----------------------------------------------

    ctx->running = true;

    ctx->worker_thread = CreateThread(
        NULL,
        0,
        event_listener_thread,
        ctx,
        0,
        NULL
    );

    if (!ctx->worker_thread) {
        ctx->running = false; // ������� ����, ���� �� ������� ������� �����
        return SHM_ERROR_MEMORY;
    }

    return SHM_SUCCESS;
}

void shm_event_stop_listener(shm_event_ctx_t* ctx) {
    if (!ctx || !ctx->running) {
        return;
    }

    ctx->running = false;

    if (ctx->worker_thread) {
        WaitForSingleObject(ctx->worker_thread, INFINITE);
        CloseHandle(ctx->worker_thread);
        ctx->worker_thread = NULL;
    }
}