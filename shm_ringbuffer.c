#include "../include/shm_ringbuffer.h"
#include <stdio.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static uint32_t next_power_of_2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

shm_ring_t* shm_ring_create(const char* name, const shm_ring_config_t* config) {
    if (!name || !config || config->size == 0) {
        return NULL;
    }

    shm_ring_t* ring = (shm_ring_t*)calloc(1, sizeof(shm_ring_t));
    if (!ring) {
        return NULL;
    }

    // ��������, ��� ������ ������ - ������� ������
    uint32_t buffer_size = next_power_of_2(config->size);
    if (buffer_size != config->size) {
        fprintf(stderr, "Warning: Buffer size adjusted to next power of 2: %u\n", buffer_size);
    }
    uint32_t total_size = sizeof(shm_ring_header_t) + buffer_size;

    char mapping_name[300];
    snprintf(mapping_name, sizeof(mapping_name), "Local\\SHM_%s", name);

    ring->file_mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        total_size,
        mapping_name
    );

    if (!ring->file_mapping) {
        free(ring);
        return NULL;
    }

    void* mapped_memory = MapViewOfFile(
        ring->file_mapping,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        total_size
    );

    if (!mapped_memory) {
        CloseHandle(ring->file_mapping);
        free(ring);
        return NULL;
    }

    ring->header = (shm_ring_header_t*)mapped_memory;
    ring->buffer = (uint8_t*)mapped_memory + sizeof(shm_ring_header_t);
    ring->is_server = true;
    ring->buffer_size = buffer_size;

    // ���������� InterlockedExchange ��� ��������� �������������
    InterlockedExchange((LONG*)&ring->header->write_pos, 0);
    InterlockedExchange((LONG*)&ring->header->read_pos, 0);
    ring->header->size = buffer_size;
    ring->header->mask = buffer_size - 1;
    InterlockedExchange((LONG*)&ring->header->active_readers, 0);
    InterlockedExchange((LONG*)&ring->header->sequence, 0);

    char event_data_name[300];
    char event_space_name[300];
    snprintf(event_data_name, sizeof(event_data_name), "Local\\SHM_DATA_%s", name);
    snprintf(event_space_name, sizeof(event_space_name), "Local\\SHM_SPACE_%s", name);

    ring->data_event = CreateEventA(NULL, FALSE, FALSE, event_data_name);
    ring->space_event = CreateEventA(NULL, FALSE, FALSE, event_space_name);

    if (!ring->data_event || !ring->space_event) {
        shm_ring_close(ring);
        return NULL;
    }

    ring->is_blocking = config->blocking;

    return ring;
}

shm_ring_t* shm_ring_open(const char* name) {
    if (!name) {
        return NULL;
    }

    shm_ring_t* ring = (shm_ring_t*)calloc(1, sizeof(shm_ring_t));
    if (!ring) {
        return NULL;
    }

    char mapping_name[300];
    snprintf(mapping_name, sizeof(mapping_name), "Local\\SHM_%s", name);

    ring->file_mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapping_name);
    if (!ring->file_mapping) {
        free(ring);
        return NULL;
    }

    void* mapped_memory = MapViewOfFile(
        ring->file_mapping,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        0
    );

    if (!mapped_memory) {
        CloseHandle(ring->file_mapping);
        free(ring);
        return NULL;
    }

    ring->header = (shm_ring_header_t*)mapped_memory;
    ring->buffer = (uint8_t*)mapped_memory + sizeof(shm_ring_header_t);
    ring->is_server = false;
    ring->buffer_size = ring->header->size;

    char event_data_name[300];
    char event_space_name[300];
    snprintf(event_data_name, sizeof(event_data_name), "Local\\SHM_DATA_%s", name);
    snprintf(event_space_name, sizeof(event_space_name), "Local\\SHM_SPACE_%s", name);

    ring->data_event = OpenEventA(EVENT_ALL_ACCESS, FALSE, event_data_name);
    ring->space_event = OpenEventA(EVENT_ALL_ACCESS, FALSE, event_space_name);

    if (!ring->data_event || !ring->space_event) {
        shm_ring_close(ring);
        return NULL;
    }

    InterlockedIncrement((LONG*)&ring->header->active_readers);

    // ����� ���������� �� ��������� ��� ��������
    ring->is_blocking = true; // ��� ����� ���������� config ����, �� ��� �������� - true

    return ring;
}

void shm_ring_close(shm_ring_t* ring) {
    if (!ring) {
        return;
    }

    if (!ring->is_server && ring->header) {
        InterlockedDecrement((LONG*)&ring->header->active_readers);
    }

    if (ring->data_event) {
        CloseHandle(ring->data_event);
    }

    if (ring->space_event) {
        CloseHandle(ring->space_event);
    }

    if (ring->header) {
        UnmapViewOfFile(ring->header);
    }

    if (ring->file_mapping) {
        CloseHandle(ring->file_mapping);
    }

    free(ring);
}

// ��������������� ������� ��� �������� ������� � ��������� (� �������������)
static DWORD wait_for_event(HANDLE event, DWORD timeout_ms) {
    DWORD result = WaitForSingleObject(event, timeout_ms);
    if (result == WAIT_FAILED) {
        // � ������ ������ ��������, ����� ����������, �� �� ��������� ������
        // ���������� ��������� ��� ��������� � ���������� �������
    }
    return result;
}

shm_error_t shm_ring_write(shm_ring_t* ring, const void* data, uint32_t size) {
    if (!ring || !data || size == 0) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // �������� ������������� ������� ����������� ���������
    if (size > SHM_MAX_EMBEDDED_MESSAGE_SIZE) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // ��������, ���������� �� ��������� ������� � ����� (������� ������ ���������)
    if (size + sizeof(uint32_t) > ring->buffer_size) {
        return SHM_ERROR_INVALID_PARAM;
    }

    // --- ����� ������: ���������� ---
    uint32_t write_pos = InterlockedExchangeAdd((LONG*)&ring->header->write_pos, 0); // ������� ������� ������
    uint32_t read_pos = InterlockedExchangeAdd((LONG*)&ring->header->read_pos, 0);   // ������� ������� ������

    // ���������, ������� ����� �����
    uint32_t required_space = sizeof(uint32_t) + size;

    // ���� ��� ������������ ����� (���������� ������ ������)
    while ((write_pos - read_pos) + required_space > ring->buffer_size) {
        // ��������� ������ ������ ������� ���������
        uint32_t current_read_idx = read_pos & ring->header->mask;
        uint32_t msg_size_to_drop;

        // ������ ������ ��������� (���������� shm_ring_read, �� ��� ������ read_pos)
        uint32_t first_chunk = MIN(sizeof(uint32_t), ring->buffer_size - current_read_idx);
        memcpy(&msg_size_to_drop, ring->buffer + current_read_idx, first_chunk);
        if (first_chunk < sizeof(uint32_t)) {
            memcpy(((uint8_t*)&msg_size_to_drop) + first_chunk, ring->buffer, sizeof(uint32_t) - first_chunk);
        }

        // �������� read_pos, "����������" ��� ���������
        // �����: �������� ����� *�����* ������ �������, �� ��������.
        // ���������� CAS (Compare And Swap) ��� InterlockedExchangeAdd.
        // ����� CAS, �� ��� �������� InterlockedExchangeAdd.
        // ��������: ��� ����� �������� � �����, ���� ������ �������� ���� �������� read_pos.
        // � overwriting ������ � ����� ��������� ��� ����� ���� ���������.
        // ��� ���������� ��������� ����� ������ ������ (��������, �� overwriting).
        // ������������ ���� �������� ��� ���������.
        // ����� ���������� ������ - ������������ CAS:
        uint32_t expected_read_pos = read_pos;
        uint32_t new_read_pos = read_pos + sizeof(uint32_t) + msg_size_to_drop;
        uint32_t current_read_pos = InterlockedCompareExchange((LONG*)&ring->header->read_pos, new_read_pos, expected_read_pos);
        if (current_read_pos != expected_read_pos) {
            // �����, ��������� � ������ ����������
            read_pos = current_read_pos;
            continue; // ������� � ��������� �������� �����
        }
        read_pos = new_read_pos; // ������� �������� read_pos
    }
    // --- ����� ����� ������ ---

    // ������ ����� ����������, ����� ������
    uint32_t write_idx = write_pos & ring->header->mask;

    uint32_t first_chunk = MIN(sizeof(uint32_t), ring->buffer_size - write_idx);
    memcpy(ring->buffer + write_idx, &size, first_chunk);
    if (first_chunk < sizeof(uint32_t)) {
        memcpy(ring->buffer, ((uint8_t*)&size) + first_chunk, sizeof(uint32_t) - first_chunk);
    }

    uint32_t data_write_pos = (write_pos + sizeof(uint32_t)) & ring->header->mask;
    uint32_t first_data_chunk = MIN(size, ring->buffer_size - data_write_pos);
    memcpy(ring->buffer + data_write_pos, data, first_data_chunk);
    if (first_data_chunk < size) {
        memcpy(ring->buffer, ((uint8_t*)data) + first_data_chunk, size - first_data_chunk);
    }

    MemoryBarrier();
    // �������� ����������� write_pos
    InterlockedExchangeAdd((LONG*)&ring->header->write_pos, sizeof(uint32_t) + size);
    InterlockedIncrement((LONG*)&ring->header->sequence);

    SetEvent(ring->data_event);

    // Burst hang fix: ��������� yield
    Sleep(0);

    return SHM_SUCCESS; // ������ ����� � overwriting ������
}

shm_error_t shm_ring_read(shm_ring_t* ring, void* data, uint32_t* size) {
    if (!ring || !data || !size) {
        return SHM_ERROR_INVALID_PARAM;
    }

    DWORD timeout_ms = INFINITE;
    if (!ring->is_blocking) {
        timeout_ms = 0; // ������������� ����� - ����� ���������� ���������
    }
    // else: ����������� �����, ���

    while (1) {
        uint32_t write_pos = ring->header->write_pos; // volatile ������
        uint32_t read_pos = ring->header->read_pos;   // volatile ������

        if (write_pos != read_pos) {
            // ���� ������, ����� ������
            break;
        }
        else {
            if (!ring->is_blocking) {
                return SHM_ERROR_EMPTY;
            }
            else {
                // ������� ��������� ������
                DWORD wait_result = wait_for_event(ring->data_event, timeout_ms);
                if (wait_result == WAIT_TIMEOUT) {
                    return SHM_ERROR_EMPTY;
                }
                else if (wait_result == WAIT_FAILED) {
                    // ������ ��������
                    return SHM_ERROR_ACCESS; // ���������� ������������ ���
                }
                // ������� ��������� �������, ���������� ����
            }
        }
    }

    // �������� �������� ������� ������� ������
    uint32_t current_read_pos = InterlockedExchangeAdd((LONG*)&ring->header->read_pos, 0);
    // ��������� �������� �� ������ ����� ����� ������ � ���� �������
    uint32_t current_write_pos = InterlockedExchangeAdd((LONG*)&ring->header->write_pos, 0);

    if (current_write_pos == current_read_pos) {
        // ���� ������ �������, ������� (������ ���� �������������)
        if (!ring->is_blocking) {
            return SHM_ERROR_EMPTY;
        }
        // �����, ������� � ����� �������� (���� ��� ������������)
        // �� ��������, ���� �� ����� �� ����� ��������, ������ ������ ����.
        // ��������, ����� ��������� ���� ��������.
        // ��� ��������, �����������, ��� ������ ����.
    }

    uint32_t read_idx = current_read_pos & ring->header->mask;
    uint32_t msg_size;

    uint32_t first_chunk = MIN(sizeof(uint32_t), ring->buffer_size - read_idx);
    memcpy(&msg_size, ring->buffer + read_idx, first_chunk);
    if (first_chunk < sizeof(uint32_t)) {
        memcpy(((uint8_t*)&msg_size) + first_chunk, ring->buffer, sizeof(uint32_t) - first_chunk);
    }

    if (msg_size > *size) {
        return SHM_ERROR_INVALID_PARAM;
    }

    uint32_t data_read_pos = (current_read_pos + sizeof(uint32_t)) & ring->header->mask;
    uint32_t first_data_chunk = MIN(msg_size, ring->buffer_size - data_read_pos);
    memcpy(data, ring->buffer + data_read_pos, first_data_chunk);
    if (first_data_chunk < msg_size) {
        memcpy(((uint8_t*)data) + first_data_chunk, ring->buffer, msg_size - first_data_chunk);
    }

    *size = msg_size;

    MemoryBarrier();
    // �������� ����������� read_pos (���������� �� ������ ��������� + ������)
    uint32_t bytes_read = sizeof(uint32_t) + msg_size;
    InterlockedExchangeAdd((LONG*)&ring->header->read_pos, bytes_read);

    SetEvent(ring->space_event); // �������������, ��� ����� ������������ (����� ���� �����������)

    // Burst hang fix: ��������� yield
    Sleep(0);

    return SHM_SUCCESS;
}

shm_error_t shm_ring_peek(shm_ring_t* ring, void* data, uint32_t* size) {
    if (!ring || !data || !size) {
        return SHM_ERROR_INVALID_PARAM;
    }

    uint32_t write_pos = ring->header->write_pos; // volatile ������
    uint32_t read_pos = ring->header->read_pos;   // volatile ������

    if (write_pos == read_pos) {
        return SHM_ERROR_EMPTY;
    }

    uint32_t read_idx = read_pos & ring->header->mask;
    uint32_t msg_size;

    uint32_t first_chunk = MIN(sizeof(uint32_t), ring->buffer_size - read_idx);
    memcpy(&msg_size, ring->buffer + read_idx, first_chunk);
    if (first_chunk < sizeof(uint32_t)) {
        memcpy(((uint8_t*)&msg_size) + first_chunk, ring->buffer, sizeof(uint32_t) - first_chunk);
    }

    if (msg_size > *size) {
        return SHM_ERROR_INVALID_PARAM;
    }

    uint32_t data_read_pos = (read_pos + sizeof(uint32_t)) & ring->header->mask;
    uint32_t first_data_chunk = MIN(msg_size, ring->buffer_size - data_read_pos);
    memcpy(data, ring->buffer + data_read_pos, first_data_chunk);
    if (first_data_chunk < msg_size) {
        memcpy(((uint8_t*)data) + first_data_chunk, ring->buffer, msg_size - first_data_chunk);
    }

    *size = msg_size;

    return SHM_SUCCESS;
}

uint32_t shm_ring_available(const shm_ring_t* ring) {
    if (!ring) {
        return 0;
    }

    uint32_t write_pos = ring->header->write_pos; // volatile ������
    uint32_t read_pos = ring->header->read_pos;   // volatile ������

    return write_pos - read_pos;
}

uint32_t shm_ring_free_space(const shm_ring_t* ring) {
    if (!ring) {
        return 0;
    }

    uint32_t write_pos = ring->header->write_pos; // volatile ������
    uint32_t read_pos = ring->header->read_pos;   // volatile ������

    return ring->buffer_size - (write_pos - read_pos);
}

bool shm_ring_is_empty(const shm_ring_t* ring) {
    if (!ring) {
        return true;
    }

    return ring->header->write_pos == ring->header->read_pos;
}

bool shm_ring_is_full(const shm_ring_t* ring) {
    if (!ring) {
        return false;
    }

    uint32_t write_pos = ring->header->write_pos; // volatile ������
    uint32_t read_pos = ring->header->read_pos;   // volatile ������

    return (write_pos - read_pos) >= ring->buffer_size;
}