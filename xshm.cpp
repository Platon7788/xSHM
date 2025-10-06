#include "xshm.hpp"

namespace xshm {

// ============================================================================
// UltimateSharedMemory Implementation
// ============================================================================

UltimateSharedMemory::UltimateSharedMemory(const std::string& name, size_t size, bool create)
    : ptr_(nullptr), size_(size), name_(name), is_owner_(create),
      hMutex_(INVALID_HANDLE_VALUE), hEvent_(INVALID_HANDLE_VALUE) {
    
    // Validate input parameters with default config
    // For clients (create=false), don't validate size as it might be smaller than min_buffer_size
    if (create) {
        XSHMValidator::validate_or_throw(name, size, XSHMConfig{});
    } else {
        XSHMValidator::validate_or_throw(name, 0, XSHMConfig{}); // Skip size validation for clients
    }
    
    // Don't apply next_power_of_2 here - size should already be properly calculated
    // size_ = next_power_of_2(size);
    
    // Helper lambda for cleanup on exception
    auto cleanup = [this]() {
        if (hMapFile_) { CloseHandle(hMapFile_); hMapFile_ = nullptr; }
        // HandleGuard members will automatically close their handles
    };
    
    try {
        if (create) {
            hMapFile_ = CreateFileMappingA(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE | SEC_COMMIT,
                0,
                static_cast<DWORD>(size_),
                name_.c_str()
            );
            if (!hMapFile_) {
                DWORD error = GetLastError();
                throw XSHMException("Failed to create shared memory: " + name_ + " (Error: " + std::to_string(error) + ")");
            }
            
            HANDLE hMutex = CreateMutexA(nullptr, FALSE, (name_ + "_mutex").c_str());
            HANDLE hEvent = CreateEventA(nullptr, FALSE, FALSE, (name_ + "_event").c_str());
            
            if (!hMutex || !hEvent) {
                DWORD error = GetLastError();
                if (hMutex) CloseHandle(hMutex);
                if (hEvent) CloseHandle(hEvent);
                throw XSHMException("Failed to create synchronization objects (Error: " + std::to_string(error) + ")");
            }
            
            hMutex_ = HandleGuard(hMutex);
            hEvent_ = HandleGuard(hEvent);
        } else {
            hMapFile_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name_.c_str());
            if (!hMapFile_) {
                DWORD error = GetLastError();
                throw XSHMException("Failed to open shared memory: " + name_ + " (Error: " + std::to_string(error) + ")");
            }
            
            HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, (name_ + "_mutex").c_str());
            HANDLE hEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, (name_ + "_event").c_str());
            
            if (!hMutex || !hEvent) {
                DWORD error = GetLastError();
                if (hMutex) CloseHandle(hMutex);
                if (hEvent) CloseHandle(hEvent);
                throw XSHMException("Failed to open synchronization objects (Error: " + std::to_string(error) + ")");
            }
            
            hMutex_ = HandleGuard(hMutex);
            hEvent_ = HandleGuard(hEvent);
        }
        
        
        ptr_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, size_);
        if (!ptr_) {
            cleanup();
            DWORD error = GetLastError();
            throw XSHMException("Failed to map shared memory view (Error: " + std::to_string(error) + ")");
        }
    } catch (...) {
        cleanup();
        throw;
    }
}

UltimateSharedMemory::~UltimateSharedMemory() {
    if (ptr_) {
        UnmapViewOfFile(ptr_);
    }
    if (hMapFile_) {
        CloseHandle(hMapFile_);
    }
    // HandleGuard members will automatically close their handles
}

UltimateSharedMemory::UltimateSharedMemory(UltimateSharedMemory&& other) noexcept
    : ptr_(other.ptr_), size_(other.size_), name_(std::move(other.name_)), is_owner_(other.is_owner_),
      hMutex_(std::move(other.hMutex_)), hEvent_(std::move(other.hEvent_)) {
    // Move hMapFile_ (HANDLE is just a pointer, so we can move it directly)
    hMapFile_ = other.hMapFile_;
    
    // Reset other object to prevent double cleanup
    other.hMapFile_ = nullptr;
    other.ptr_ = nullptr;
    other.size_ = 0;
    other.is_owner_ = false;
}

UltimateSharedMemory& UltimateSharedMemory::operator=(UltimateSharedMemory&& other) noexcept {
    if (this != &other) {
        this->~UltimateSharedMemory();
        new (this) UltimateSharedMemory(std::move(other));
    }
    return *this;
}


void UltimateSharedMemory::signal() {
    if (hEvent_.handle && hEvent_.handle != INVALID_HANDLE_VALUE) {
        SetEvent(hEvent_.handle);
    }
}

void UltimateSharedMemory::wait(std::chrono::milliseconds timeout) {
    if (hEvent_.handle && hEvent_.handle != INVALID_HANDLE_VALUE) {
        DWORD result = WaitForSingleObject(hEvent_.handle, timeout.count() ? static_cast<DWORD>(timeout.count()) : INFINITE);
        if (result == WAIT_TIMEOUT) {
            throw XSHMException("Wait timeout");
        } else if (result != WAIT_OBJECT_0) {
            throw XSHMException("Wait failed");
        }
    }
}

constexpr size_t UltimateSharedMemory::next_power_of_2(size_t n) noexcept {
    if (n == 0) return 1;
    if (n > (UINT32_MAX >> 1)) {
        // Prevent overflow for very large values
        return UINT32_MAX;
    }
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if (sizeof(size_t) > 4) {
        n |= n >> 32;
    }
    return n + 1;
}

// ============================================================================
// RingBuffer Implementation
// ============================================================================

template<typename T>
RingBuffer<T>::RingBuffer(void* memory, BufferSize capacity) 
    : capacity_(next_power_of_2(capacity))
    , mask_(capacity_ - 1)
    , data_(static_cast<T*>(memory)) {
    
    if (capacity_ < DEFAULT_MIN_BUFFER_SIZE || capacity_ > DEFAULT_MAX_BUFFER_SIZE) {
        throw XSHMException("Invalid buffer size: " + std::to_string(capacity));
    }
    
    // Initialize atomic variables
    write_pos_.store(0, XSHM_RELEASE);
    read_pos_.store(0, XSHM_RELEASE);
    read_sequence_.store(0, XSHM_RELEASE);
    
    // Инициализируем память нулями
    std::memset(data_, 0, capacity_ * sizeof(T));
    
    // Сбрасываем статистику
    reset_statistics();
}

template<typename T>
bool RingBuffer<T>::try_write(const T& item) noexcept {
    return try_write_impl([&item](T& dest) { dest = item; });
}

template<typename T>
bool RingBuffer<T>::try_write(T&& item) noexcept {
    return try_write_impl([&item](T& dest) { dest = std::move(item); });
}

template<typename T>
template<typename... Args>
bool RingBuffer<T>::try_emplace(Args&&... args) noexcept {
    return try_write_impl([&args...](T& dest) { new (&dest) T(std::forward<Args>(args)...); });
}

template<typename T>
T* RingBuffer<T>::try_read() noexcept {
    // Check if data is available without advancing position
    const BufferIndex current_read = read_pos_.load(XSHM_RELAXED);
    const BufferIndex current_write = write_pos_.load(XSHM_ACQUIRE);
    
    // Check if buffer is empty
    if (current_read == current_write) {
        failed_reads_.fetch_add(1, XSHM_RELAXED);
        return nullptr;
    }
    
    // Return pointer to data (position will be advanced in commit_read)
    return &data_[current_read];
}

template<typename T>
T* RingBuffer<T>::try_read(uint64_t& sequence) noexcept {
    // Check if data is available without advancing position
    const BufferIndex current_read = read_pos_.load(XSHM_RELAXED);
    const BufferIndex current_write = write_pos_.load(XSHM_ACQUIRE);
    
    // Check if buffer is empty
    if (current_read == current_write) {
        failed_reads_.fetch_add(1, XSHM_RELAXED);
        return nullptr;
    }
    
    // Get current sequence number for verification
    sequence = read_sequence_.load(XSHM_RELAXED);
    
    // Return pointer to data (position will be advanced in commit_read)
    return &data_[current_read];
}

template<typename T>
bool RingBuffer<T>::commit_read(uint64_t expected_sequence) noexcept {
    // CAS-loop for atomic read position advancement with sequence verification and backoff
    BufferIndex current_read = read_pos_.load(XSHM_RELAXED);
    BufferIndex next_read;
    int spins = 0;
    
    do {
        // Verify sequence number matches
        const uint64_t current_sequence = read_sequence_.load(XSHM_RELAXED);
        if (current_sequence != expected_sequence) {
            return false; // Sequence mismatch - invalid commit
        }
        
        next_read = (current_read + 1) & mask_;
        
        // Try to atomically advance read position and sequence
        // If CAS fails, current_read will be updated to actual value
        if (++spins > 16) {
            std::this_thread::yield(); // Yield CPU to prevent busy-wait
            spins = 0;
        }
    } while (!read_pos_.compare_exchange_weak(current_read, next_read, XSHM_RELEASE, XSHM_ACQUIRE));
    
    // Advance sequence number atomically
    read_sequence_.fetch_add(1, XSHM_RELEASE);
    total_reads_.fetch_add(1, XSHM_RELAXED);
    
    return true;
}

template<typename T>
bool RingBuffer<T>::commit_read() noexcept {
    // CAS-loop for atomic read position advancement without sequence verification (legacy)
    BufferIndex current_read = read_pos_.load(XSHM_RELAXED);
    BufferIndex next_read;
    int spins = 0;
    
    do {
        next_read = (current_read + 1) & mask_;
        
        // Try to atomically advance read position
        if (++spins > 16) {
            std::this_thread::yield(); // Yield CPU to prevent busy-wait
            spins = 0;
        }
    } while (!read_pos_.compare_exchange_weak(current_read, next_read, XSHM_RELEASE, XSHM_ACQUIRE));
    
    // Advance sequence number atomically
    read_sequence_.fetch_add(1, XSHM_RELEASE);
    total_reads_.fetch_add(1, XSHM_RELAXED);
    
    return true;
}

template<typename T>
void RingBuffer<T>::initialize_for_shared_memory(BufferSize buffer_size) noexcept {
    write_pos_.store(0, XSHM_RELEASE);
    read_pos_.store(0, XSHM_RELEASE);
    read_sequence_.store(0, XSHM_RELEASE);
    // Note: capacity_ and mask_ are already set in constructor
}

template<typename T>
void RingBuffer<T>::set_capacity(BufferSize capacity) noexcept {
    capacity_ = capacity;
    mask_ = capacity - 1;
}

template<typename T>
bool RingBuffer<T>::empty() const noexcept {
    return write_pos_.load(XSHM_ACQUIRE) == read_pos_.load(XSHM_ACQUIRE);
}

template<typename T>
bool RingBuffer<T>::full() const noexcept {
    const BufferIndex current_write = write_pos_.load(XSHM_ACQUIRE);
    const BufferIndex current_read = read_pos_.load(XSHM_ACQUIRE);
    const BufferIndex next_write = (current_write + 1) & mask_;
    return next_write == current_read;
}

template<typename T>
BufferSize RingBuffer<T>::size() const noexcept {
    const BufferIndex current_write = write_pos_.load(XSHM_ACQUIRE);
    const BufferIndex current_read = read_pos_.load(XSHM_ACQUIRE);
    return (current_write - current_read) & mask_;
}

template<typename T>
BufferSize RingBuffer<T>::capacity() const noexcept {
    return capacity_ - 1;  // Один слот всегда свободен для различения полного/пустого состояния
}

template<typename T>
uint64_t RingBuffer<T>::total_writes() const noexcept {
    return total_writes_.load(XSHM_RELAXED);
}

template<typename T>
uint64_t RingBuffer<T>::total_reads() const noexcept {
    return total_reads_.load(XSHM_RELAXED);
}

template<typename T>
uint64_t RingBuffer<T>::failed_writes() const noexcept {
    return failed_writes_.load(XSHM_RELAXED);
}

template<typename T>
uint64_t RingBuffer<T>::failed_reads() const noexcept {
    return failed_reads_.load(XSHM_RELAXED);
}

template<typename T>
void RingBuffer<T>::reset_statistics() noexcept {
    total_writes_.store(0, XSHM_RELAXED);
    total_reads_.store(0, XSHM_RELAXED);
    failed_writes_.store(0, XSHM_RELAXED);
    failed_reads_.store(0, XSHM_RELAXED);
}

template<typename T>
constexpr BufferSize RingBuffer<T>::next_power_of_2(BufferSize n) noexcept {
    if (n == 0) return 1;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

template<typename T>
BufferIndex RingBuffer<T>::next_write_pos() const noexcept {
    return (write_pos_.load(XSHM_RELAXED) + 1) & mask_;
}

template<typename T>
BufferIndex RingBuffer<T>::next_read_pos() const noexcept {
    return (read_pos_.load(XSHM_RELAXED) + 1) & mask_;
}

// ============================================================================
// DualRingBufferSystem Implementation
// ============================================================================

template<typename T>
DualRingBufferSystem<T>::DualRingBufferSystem(std::string name, BufferSize buffer_size, bool is_server, const XSHMConfig& config)
    : name_(std::move(name)), is_server_(is_server), 
      hMutex_(INVALID_HANDLE_VALUE), hEventServerToClient_(INVALID_HANDLE_VALUE), 
      hEventClientToServer_(INVALID_HANDLE_VALUE), hEventConnection_(INVALID_HANDLE_VALUE) {
    
    // Validate input parameters (use name_ after move)
    XSHMValidator::validate_or_throw(name_, buffer_size, config);
    
    // Вычисляем размер разделяемой памяти
    const size_t header_size = sizeof(SharedMemoryHeader);
    const size_t buffer_metadata_size = sizeof(RingBuffer<T>) * 2;
    
    if (is_server) {
        // Server creates shared memory with specified buffer size
        // Use next_power_of_2 to match RingBuffer's internal calculation
        const BufferSize actual_buffer_size = UltimateSharedMemory::next_power_of_2(buffer_size);
        const size_t buffer_data_size = actual_buffer_size * sizeof(T) * 2;
        const size_t total_size = UltimateSharedMemory::next_power_of_2(header_size + buffer_metadata_size + buffer_data_size);
        
        // Создаем разделяемую память
        shm_ = std::make_unique<UltimateSharedMemory>(name_, total_size, true);
        layout_ = static_cast<SharedLayout*>(shm_->get());
        
        // Инициализируем заголовок с placement new
        new (&layout_->header) SharedMemoryHeader();
        layout_->header.magic_number.store(MAGIC_NUMBER, XSHM_RELEASE);
        layout_->header.version.store(PROTOCOL_VERSION, XSHM_RELEASE);
        layout_->header.server_to_client_buffer_size.store(actual_buffer_size, XSHM_RELEASE);
        layout_->header.client_to_server_buffer_size.store(actual_buffer_size, XSHM_RELEASE);
        layout_->header.server_connected.store(0, XSHM_RELEASE);
        layout_->header.client_connected.store(0, XSHM_RELEASE);
        
        // Инициализируем буферы с placement new
        new (&layout_->server_to_client_buffer) RingBuffer<T>();
        new (&layout_->client_to_server_buffer) RingBuffer<T>();
        
        // Устанавливаем capacity и data pointers
        layout_->server_to_client_buffer.set_capacity(actual_buffer_size);
        layout_->client_to_server_buffer.set_capacity(actual_buffer_size);
        
        // Устанавливаем data pointers для буферов
        layout_->server_to_client_buffer.set_data_pointer(reinterpret_cast<T*>(&layout_->server_to_client_data[0]));
        layout_->client_to_server_buffer.set_data_pointer(reinterpret_cast<T*>(&layout_->client_to_server_data[0]));
        
        // Инициализируем data области нулями для безопасности
        std::memset(&layout_->server_to_client_data[0], 0, actual_buffer_size * sizeof(T));
        std::memset(&layout_->client_to_server_data[0], 0, actual_buffer_size * sizeof(T));
        
        // Создаем объекты синхронизации
        HANDLE hMutex = CreateMutexA(nullptr, FALSE, (name_ + "_mutex").c_str());
        HANDLE hEventSxC = CreateEventA(nullptr, FALSE, FALSE, (name_ + "_event_server_to_client").c_str());
        HANDLE hEventCxS = CreateEventA(nullptr, FALSE, FALSE, (name_ + "_event_client_to_server").c_str());
        HANDLE hEventConn = CreateEventA(nullptr, FALSE, FALSE, (name_ + "_event_conn").c_str());
        
        if (!hMutex || !hEventSxC || !hEventCxS || !hEventConn) {
            DWORD error = GetLastError();
            // Cleanup on error
            if (hMutex) CloseHandle(hMutex);
            if (hEventSxC) CloseHandle(hEventSxC);
            if (hEventCxS) CloseHandle(hEventCxS);
            if (hEventConn) CloseHandle(hEventConn);
            throw XSHMException("Failed to create synchronization objects (Error: " + std::to_string(error) + ")");
        }
        
        // Move handles to HandleGuard members
        hMutex_ = HandleGuard(hMutex);
        hEventServerToClient_ = HandleGuard(hEventSxC);
        hEventClientToServer_ = HandleGuard(hEventCxS);
        hEventConnection_ = HandleGuard(hEventConn);
        
        // Сигнализируем о подключении сервера
        layout_->header.server_connected.store(1, XSHM_RELEASE);
        signal_connection();
        
    } else {
        // Client opens existing shared memory with proper error handling
        try {
            // First, try to open with minimal size to read header
            shm_ = std::make_unique<UltimateSharedMemory>(name_, sizeof(SharedMemoryHeader), false);
            layout_ = static_cast<SharedLayout*>(shm_->get());
            
            // Validate header integrity atomically before calculating total size
            if (!XSHMTOCTOUProtection::validate_integrity_atomic(layout_, config)) {
                shm_.reset();
                layout_ = nullptr;
                throw XSHMException("Server not found or corrupted header - validation failed");
            }
            
            // Read actual size from header
            const BufferSize sxc_size = layout_->header.server_to_client_buffer_size.load(XSHM_ACQUIRE);
            const BufferSize cxs_size = layout_->header.client_to_server_buffer_size.load(XSHM_ACQUIRE);
            
            // Validate buffer sizes are consistent
            if (sxc_size != cxs_size) {
                shm_.reset();
                layout_ = nullptr;
                throw XSHMException("Server not found or corrupted header - buffer size mismatch");
            }
            
            const size_t buffer_data_size = sxc_size * sizeof(T) * 2;
            const size_t total_size = UltimateSharedMemory::next_power_of_2(header_size + buffer_metadata_size + buffer_data_size);
            
            // Validate total size to prevent overflow
            if (total_size < header_size + buffer_metadata_size || 
                total_size > SIZE_MAX - buffer_metadata_size ||
                buffer_data_size > SIZE_MAX - header_size - buffer_metadata_size) {
                shm_.reset();
                layout_ = nullptr;
                throw XSHMException("Server not found or corrupted header - buffer size overflow");
            }
            
            // Additional overflow check for multiplication
            if (sxc_size > SIZE_MAX / sizeof(T) / 2) {
                shm_.reset();
                layout_ = nullptr;
                throw XSHMException("Server not found or corrupted header - buffer size multiplication overflow");
            }
            
            // Close current mapping and reopen with correct size
            shm_.reset();
            shm_ = std::make_unique<UltimateSharedMemory>(name_, total_size, false);
            layout_ = static_cast<SharedLayout*>(shm_->get());
            
            // Re-validate after reopen to prevent TOCTOU attacks
            if (!XSHMTOCTOUProtection::validate_integrity_atomic(layout_, config)) {
                shm_.reset();
                layout_ = nullptr;
                throw XSHMException("Shared memory validation failed after reopen - possible TOCTOU attack");
            }
            
        } catch (const std::exception& e) {
            // Cleanup on error
            shm_.reset();
            layout_ = nullptr;
            throw XSHMException(std::string("Failed to connect to server: ") + e.what());
        }
        
        // Открываем объекты синхронизации
        HANDLE hMutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, (name_ + "_mutex").c_str());
        HANDLE hEventSxC = OpenEventA(EVENT_ALL_ACCESS, FALSE, (name_ + "_event_server_to_client").c_str());
        HANDLE hEventCxS = OpenEventA(EVENT_ALL_ACCESS, FALSE, (name_ + "_event_client_to_server").c_str());
        HANDLE hEventConn = OpenEventA(EVENT_ALL_ACCESS, FALSE, (name_ + "_event_conn").c_str());
        
        if (!hMutex || !hEventSxC || !hEventCxS || !hEventConn) {
            DWORD error = GetLastError();
            // Cleanup on error
            if (hMutex) CloseHandle(hMutex);
            if (hEventSxC) CloseHandle(hEventSxC);
            if (hEventCxS) CloseHandle(hEventCxS);
            if (hEventConn) CloseHandle(hEventConn);
            throw XSHMException("Failed to open synchronization objects (Error: " + std::to_string(error) + ")");
        }
        
        // Move handles to HandleGuard members
        hMutex_ = HandleGuard(hMutex);
        hEventServerToClient_ = HandleGuard(hEventSxC);
        hEventClientToServer_ = HandleGuard(hEventCxS);
        hEventConnection_ = HandleGuard(hEventConn);
        
        // Инициализируем буферы с placement new (client)
        new (&layout_->server_to_client_buffer) RingBuffer<T>();
        new (&layout_->client_to_server_buffer) RingBuffer<T>();
        
        // Устанавливаем capacity и data pointers для client
        const BufferSize sxc_size = layout_->header.server_to_client_buffer_size.load(XSHM_ACQUIRE);
        const BufferSize cxs_size = layout_->header.client_to_server_buffer_size.load(XSHM_ACQUIRE);
        layout_->server_to_client_buffer.set_capacity(sxc_size);
        layout_->client_to_server_buffer.set_capacity(cxs_size);
        
        // Устанавливаем data pointers для буферов
        layout_->server_to_client_buffer.set_data_pointer(reinterpret_cast<T*>(&layout_->server_to_client_data[0]));
        layout_->client_to_server_buffer.set_data_pointer(reinterpret_cast<T*>(&layout_->client_to_server_data[0]));
        
        // Инициализируем data области нулями для безопасности
        std::memset(&layout_->server_to_client_data[0], 0, sxc_size * sizeof(T));
        std::memset(&layout_->client_to_server_data[0], 0, cxs_size * sizeof(T));
        
        // Сигнализируем о подключении клиента
        layout_->header.client_connected.store(1, XSHM_RELEASE);
        signal_connection();
    }
    
    // Устанавливаем указатели на буферы
    server_to_client_buffer_ = &layout_->server_to_client_buffer;
    client_to_server_buffer_ = &layout_->client_to_server_buffer;
}


template<typename T>
DualRingBufferSystem<T>::~DualRingBufferSystem() {
    try {
        // Signal connection loss only if currently connected
        if (layout_) {
            bool was_connected = false;
            if (is_server_) {
                was_connected = layout_->header.server_connected.load(XSHM_ACQUIRE) != 0;
                if (was_connected) {
                    layout_->header.server_connected.store(0, XSHM_RELEASE);
                }
            } else {
                was_connected = layout_->header.client_connected.load(XSHM_ACQUIRE) != 0;
                if (was_connected) {
                    layout_->header.client_connected.store(0, XSHM_RELEASE);
                }
            }
            
            if (was_connected) {
                // Memory fence to ensure visibility before signaling
                std::atomic_thread_fence(std::memory_order_seq_cst);
                signal_connection();
            }
        }
        
        // HandleGuard members will automatically close handles in their destructors
        // No manual cleanup needed - RAII handles everything
        layout_ = nullptr;
        
    } catch (...) {
        // Destructor should not throw - log error if possible
        // In production, you might want to use a logging system here
        // For now, we silently handle the exception to prevent std::terminate
    }
}

template<typename T>
void DualRingBufferSystem<T>::signal_server_to_client() noexcept {
    if (hEventServerToClient_.handle && hEventServerToClient_.handle != INVALID_HANDLE_VALUE) {
        SetEvent(hEventServerToClient_.handle);
    }
}

template<typename T>
void DualRingBufferSystem<T>::signal_client_to_server() noexcept {
    if (hEventClientToServer_.handle && hEventClientToServer_.handle != INVALID_HANDLE_VALUE) {
        SetEvent(hEventClientToServer_.handle);
    }
}

template<typename T>
void DualRingBufferSystem<T>::signal_connection() noexcept {
    if (hEventConnection_.handle && hEventConnection_.handle != INVALID_HANDLE_VALUE) {
        SetEvent(hEventConnection_.handle);
    }
}


template<typename T>
typename DualRingBufferSystem<T>::Statistics DualRingBufferSystem<T>::get_statistics() const noexcept {
    Statistics stats{};  // Initialize with zero values
    stats.server_to_client_writes = server_to_client_buffer_->total_writes();
    stats.server_to_client_reads = server_to_client_buffer_->total_reads();
    stats.server_to_client_failed_writes = server_to_client_buffer_->failed_writes();
    stats.server_to_client_failed_reads = server_to_client_buffer_->failed_reads();
    
    stats.client_to_server_writes = client_to_server_buffer_->total_writes();
    stats.client_to_server_reads = client_to_server_buffer_->total_reads();
    stats.client_to_server_failed_writes = client_to_server_buffer_->failed_writes();
    stats.client_to_server_failed_reads = client_to_server_buffer_->failed_reads();
    
    return stats;
}

template<typename T>
void DualRingBufferSystem<T>::reset_statistics() noexcept {
    server_to_client_buffer_->reset_statistics();
    client_to_server_buffer_->reset_statistics();
}

template<typename T>
void RingBuffer<T>::set_data_pointer(T* data_ptr) noexcept {
    data_ = data_ptr;
}

template<typename T>
bool DualRingBufferSystem<T>::is_server_connected() const noexcept {
    return layout_->header.server_connected.load(XSHM_ACQUIRE) != 0;
}

template<typename T>
bool DualRingBufferSystem<T>::is_client_connected() const noexcept {
    return layout_->header.client_connected.load(XSHM_ACQUIRE) != 0;
}

template<typename T>
bool DualRingBufferSystem<T>::is_connected() const noexcept {
    return this->is_server_connected() && this->is_client_connected();
}

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

// Создаем явные инстанцирования для часто используемых типов
template class RingBuffer<uint8_t>;      // unsigned char
template class RingBuffer<uint16_t>;
template class RingBuffer<uint32_t>;
template class RingBuffer<uint64_t>;
template class RingBuffer<int8_t>;       // signed char
template class RingBuffer<int16_t>;
template class RingBuffer<int32_t>;
template class RingBuffer<int64_t>;
template class RingBuffer<float>;
template class RingBuffer<double>;
template class RingBuffer<char>;

template class DualRingBufferSystem<uint8_t>;      // unsigned char
template class DualRingBufferSystem<uint16_t>;
template class DualRingBufferSystem<uint32_t>;
template class DualRingBufferSystem<uint64_t>;
template class DualRingBufferSystem<int8_t>;       // signed char
template class DualRingBufferSystem<int16_t>;
template class DualRingBufferSystem<int32_t>;
template class DualRingBufferSystem<int64_t>;
template class DualRingBufferSystem<float>;
template class DualRingBufferSystem<double>;
template class DualRingBufferSystem<char>;

template class AsyncXSHM<uint8_t>;      // unsigned char
template class AsyncXSHM<uint16_t>;
template class AsyncXSHM<uint32_t>;
template class AsyncXSHM<uint64_t>;
template class AsyncXSHM<int8_t>;       // signed char
template class AsyncXSHM<int16_t>;
template class AsyncXSHM<int32_t>;
template class AsyncXSHM<int64_t>;
template class AsyncXSHM<float>;
template class AsyncXSHM<double>;
template class AsyncXSHM<char>;

// ============================================================================
// EXPLICIT INSTANTIATIONS FOR XSHMessage
// ============================================================================

// XSHMessage uses AsyncXSHM<uint8_t> internally, so we need to ensure it's instantiated
// (it's already instantiated above, but this makes it explicit for XSHMessage)

} // namespace xshm