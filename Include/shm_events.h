#ifndef SHM_EVENTS_H
#define SHM_EVENTS_H

#include "shm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Event context structure
    typedef struct {
        HANDLE event_handles[5];           // Event objects for different event types
        shm_event_callback_t callback;     // User callback function
        void* user_data;                   // User data for callback
        HANDLE worker_thread;              // Background worker thread
        volatile bool running;             // Worker thread running flag
        uint32_t timeout_ms;               // Event timeout
        // --- НОВОЕ: Для периодической проверки статуса подключения ---
        bool is_server;                    // Флаг: сервер или клиент
        void* owner;                       // Указатель на shm_server_t или shm_client_t
        // ------------------------------------------------------------
    } shm_event_ctx_t;

    // Create event context
    SHM_API shm_event_ctx_t* shm_event_create(const char* name, bool is_server);

    // Destroy event context
    SHM_API void shm_event_destroy(shm_event_ctx_t* ctx);

    // Register event callback
    SHM_API shm_error_t shm_event_register_callback(shm_event_ctx_t* ctx,
        shm_event_callback_t callback,
        void* user_data);

    // Signal event
    SHM_API shm_error_t shm_event_signal(shm_event_ctx_t* ctx, shm_event_type_t event_type);

    // Wait for event (blocking)
    SHM_API shm_error_t shm_event_wait(shm_event_ctx_t* ctx, shm_event_type_t event_type, uint32_t timeout_ms);

    // Start event listener thread
    SHM_API shm_error_t shm_event_start_listener(shm_event_ctx_t* ctx);

    // Stop event listener thread
    SHM_API void shm_event_stop_listener(shm_event_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // SHM_EVENTS_H