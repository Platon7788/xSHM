#ifndef SHM_RINGBUFFER_H
#define SHM_RINGBUFFER_H

#include "shm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Ring buffer header structure (shared memory layout)
    typedef struct {
        volatile uint32_t write_pos;        // Write position
        volatile uint32_t read_pos;         // Read position
        volatile uint32_t size;             // Buffer size (power of 2)
        volatile uint32_t mask;             // Size - 1 (for fast modulo)
        volatile uint32_t active_readers;   // Number of active readers
        volatile uint32_t sequence;         // Sequence number for synchronization
        uint8_t padding[40];                // Cache line padding (64 bytes total)
    } shm_ring_header_t;

    // Ring buffer handle
    typedef struct {
        shm_ring_header_t* header;
        uint8_t* buffer;
        HANDLE file_mapping;
        HANDLE data_event; // —игнализирует о по€влении новых данных (даже если старые перезаписаны)
        HANDLE space_event; // ћожет быть менее актуален, но оставим дл€ совместимости
        bool is_server;
        uint32_t buffer_size;
        bool is_blocking; // –ежим блокировки дл€ чтени€
    } shm_ring_t;

    // Create ring buffer (server side)
    SHM_API shm_ring_t* shm_ring_create(const char* name, const shm_ring_config_t* config);

    // Open existing ring buffer (client side)
    SHM_API shm_ring_t* shm_ring_open(const char* name);

    // Close ring buffer
    SHM_API void shm_ring_close(shm_ring_t* ring);

    // Write data to ring buffer (zero-copy, always succeeds if size <= MAX_EMBEDDED_MESSAGE_SIZE, overwrites old data if full)
    SHM_API shm_error_t shm_ring_write(shm_ring_t* ring, const void* data, uint32_t size);

    // Read data from ring buffer (zero-copy)
    SHM_API shm_error_t shm_ring_read(shm_ring_t* ring, void* data, uint32_t* size);

    // Peek data without removing from buffer
    SHM_API shm_error_t shm_ring_peek(shm_ring_t* ring, void* data, uint32_t* size);

    // Get available data size
    SHM_API uint32_t shm_ring_available(const shm_ring_t* ring);

    // Get free space size (всегда >= 0, но не отражает перезапись)
    SHM_API uint32_t shm_ring_free_space(const shm_ring_t* ring);

    // Check if buffer is empty (read_pos == write_pos)
    SHM_API bool shm_ring_is_empty(const shm_ring_t* ring);

    // Check if buffer is full (теперь не €вл€етс€ сигналом о невозможности записи)
    SHM_API bool shm_ring_is_full(const shm_ring_t* ring);

#ifdef __cplusplus
}
#endif

#endif // SHM_RINGBUFFER_H