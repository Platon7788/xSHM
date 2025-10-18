#ifndef SHM_TYPES_H
#define SHM_TYPES_H

#ifdef _WIN32
#include <windows.h>
#define SHM_API  // ������ ��� ����������� ���������� (��� export/import)
#else
#define SHM_API
#endif

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    // --- �������� ��������� ---
#define SHM_TEST_NUM_MESSAGES 10000  // ���������� ��������� *� ������ �������*
#define SHM_TEST_MAX_RETRY_PER_MSG 3
#define SHM_TEST_MAX_DURATION_SEC 300
#define SHM_TEST_PROGRESS_INTERVAL 1000
// --------------------------------

    // --- ��������� ��� ���������� ������ ---
#define SHM_MAX_EMBEDDED_MESSAGE_SIZE 65535 // 64 KB - 1
// ---------------------------------------------

    // Error codes
    typedef enum {
        SHM_SUCCESS = 0,
        SHM_ERROR_INVALID_PARAM = -1,
        SHM_ERROR_MEMORY = -2,
        SHM_ERROR_TIMEOUT = -3,
        // SHM_ERROR_FULL ������ �� ������������ ��� write � overwriting ������
        SHM_ERROR_EMPTY = -4,
        SHM_ERROR_EXISTS = -5,
        SHM_ERROR_NOT_FOUND = -6,
        SHM_ERROR_ACCESS = -7
    } shm_error_t;

    // Ring buffer configuration
    typedef struct {
        uint32_t size;              // Buffer size in bytes (must be power of 2)
        uint32_t max_readers;       // Maximum number of readers
        bool blocking;              // Enable blocking mode (��� read, write ������ �����)
        uint32_t timeout_ms;        // Timeout for blocking operations
    } shm_ring_config_t;

    // Event types
    typedef enum {
        SHM_EVENT_DATA_AVAILABLE = 0, // ���� ������ ��� ������
        SHM_EVENT_SPACE_AVAILABLE = 1, // ����� ���� ����� �������� ��� overwriting ������
        SHM_EVENT_DISCONNECT = 2,      // ������ ����������
        SHM_EVENT_ERROR = 3,           // ����� ������
        // --- ����� ---
        SHM_EVENT_CONNECT = 4          // ������ ����������� (���������� �������������)
        // --------------
    } shm_event_type_t;

    // Callback function type
    typedef void (*shm_event_callback_t)(void* user_data, shm_event_type_t event_type, const void* data, uint32_t size);

    // Forward declarations
    typedef struct shm_server_t shm_server_t;
    typedef struct shm_client_t shm_client_t;

#ifdef __cplusplus
}
#endif

#endif // SHM_TYPES_H