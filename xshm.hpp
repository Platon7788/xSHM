#pragma once

// Visual Studio deprecation warnings configuration
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4996)  // Disable C4996 errors for deprecated functions
    #pragma warning(disable: 4293)   // Disable C4293 warnings for shift operations
#endif

// XSHM Ultimate Async Ring Buffer System by ︻┻┳══━一 Pl∀tonシ
// Version: 5.0.0 - Pure Async Model
// Compatible: C++17, C++20, C++23
// Platforms: Windows (VS 2019+, Rad Studio 13, Borland C++ Builder)
// Architectures: x86, x64 with cross-bitness compatibility

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <algorithm>


#ifdef _WIN32
    #include <windows.h>
    #include <memoryapi.h>
    #include <handleapi.h>
    #include <synchapi.h>
    #include <malloc.h>
#else
    #error "XSHM supports only Windows platform"
#endif

// Feature detection macros
#if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
        #define XSHM_NODISCARD [[nodiscard]]
    #else
        #define XSHM_NODISCARD
    #endif
#else
    #define XSHM_NODISCARD
#endif

// Cross-bitness compatibility - fixed width types
namespace xshm {
    using BufferIndex = uint32_t;      // Buffer indices (32-bit for compatibility)
    using MessageId = uint64_t;        // Message IDs
    using Timestamp = uint64_t;        // Timestamps
    using BufferSize = uint32_t;       // Buffer sizes

    // Constants
    static constexpr BufferSize DEFAULT_MIN_BUFFER_SIZE = 1024;
    static constexpr BufferSize DEFAULT_MAX_BUFFER_SIZE = 1024 * 1024;  // 1MB
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t ALIGNMENT = 64;
    static constexpr uint32_t MAGIC_NUMBER = 0x5853484D;  // "XSHM"
    static constexpr uint32_t PROTOCOL_VERSION = 1;
    
    // Configuration class for customizable limits
    struct XSHMConfig {
        BufferSize min_buffer_size = DEFAULT_MIN_BUFFER_SIZE;
        BufferSize max_buffer_size = DEFAULT_MAX_BUFFER_SIZE;
        size_t event_loop_timeout_ms = 1000;
        size_t connection_timeout_ms = 5000;
        size_t max_retry_attempts = 3;
        /** @brief Initial retry delay in milliseconds for connection attempts */
        size_t initial_retry_delay_ms = 50;
        /** @brief Maximum retry delay in milliseconds for connection attempts */
        size_t max_retry_delay_ms = 1000;
        
        // Security settings
        bool enable_toctou_protection = true;
        bool enable_integrity_checks = true;
        bool enable_version_validation = true;
        size_t max_validation_retries = 3;
        
        // Performance settings
        size_t max_batch_size = 32;           // Maximum messages to process in one batch
        size_t max_callback_timeout_ms = 10;   // Maximum time to spend in callbacks
        bool enable_batch_processing = true;   // Enable batch processing for better throughput
        bool enable_async_callbacks = true;    // Run callbacks in separate threads
        size_t callback_thread_pool_size = 4;  // Number of threads for async callbacks
        
        // Logging settings
        bool enable_logging = false;           // Enable debug logging
        bool enable_auto_reconnect = false;    // Enable automatic reconnection
        
        // Advanced settings
        bool enable_sequence_verification = true;  // Enable sequence-based read verification
        bool enable_activity_tracking = true;     // Enable activity timestamp tracking
        bool enable_performance_counters = true;   // Enable performance statistics
        bool enable_statistics = true;             // Enable statistics collection
        size_t max_cas_spins = 16;                 // Maximum CAS spins before yield
        size_t cas_yield_threshold = 16;           // CAS yield threshold for backoff
        
        // Validation
        bool is_valid() const noexcept {
            return min_buffer_size > 0 && 
                   max_buffer_size >= min_buffer_size && 
                   max_buffer_size <= (1024ULL * 1024 * 1024) && // Max 1GB
                   event_loop_timeout_ms > 0 &&
                   connection_timeout_ms > 0 &&
                   max_retry_attempts > 0 &&
                   initial_retry_delay_ms > 0 &&
                   max_retry_delay_ms >= initial_retry_delay_ms &&
                   max_batch_size > 0 &&
                   max_callback_timeout_ms > 0 &&
                   callback_thread_pool_size > 0 &&
                   // enable_batch_processing is bool, no validation needed
                   max_cas_spins > 0 &&
                   cas_yield_threshold > 0 &&
                   max_validation_retries > 0;
        }
    };
}

// Hardware interference size detection
#define XSHM_CACHE_LINE_SIZE 64

// Memory ordering shortcuts
#define XSHM_RELAXED std::memory_order_relaxed
#define XSHM_ACQUIRE std::memory_order_acquire
#define XSHM_RELEASE std::memory_order_release

namespace xshm {

// Type traits for better type safety
template<typename T>
struct TriviallySerializable : std::bool_constant<std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>> {};

template<typename T>
struct RingBufferElement : std::bool_constant<std::is_nothrow_destructible_v<T> && std::is_constructible_v<T>> {};

// Exception classes
class XSHMException : public std::runtime_error {
public:
    explicit XSHMException(const std::string& msg) : std::runtime_error("XSHM: " + msg) {}
};


/**
 * @brief Input validation utilities for XSHM
 * 
 * This class provides static methods to validate input parameters
 * for shared memory operations, ensuring they meet WinAPI requirements.
 */
class XSHMValidator {
public:
    /**
     * @brief Validates shared memory name
     * @param name The name to validate
     * @return true if name is valid, false otherwise
     * 
     * Valid names must be:
     * - 1-260 characters long
     * - Contain only alphanumeric characters, underscores, hyphens, and dots
     */
    static bool validate_name(const std::string& name) {
        if (name.empty() || name.length() > 260) return false;
        // Check for invalid characters for WinAPI
        return std::all_of(name.begin(), name.end(), [](char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';
        });
    }
    
    /**
     * @brief Validates buffer size against configuration
     * @param size The size to validate
     * @param config Configuration with size limits
     * @return true if size is within valid range, false otherwise
     * 
     * Valid sizes must be between config.min_buffer_size and config.max_buffer_size
     */
    static bool validate_buffer_size(size_t size, const XSHMConfig& config) {
        return size >= config.min_buffer_size && size <= config.max_buffer_size;
    }
    
    /**
     * @brief Validates input parameters and throws exception if invalid
     * @param name The shared memory name to validate
     * @param size The buffer size to validate (0 means no size validation)
     * @param config Configuration with validation limits
     * @throws XSHMException if validation fails
     * 
     * This method combines name and size validation and throws
     * a descriptive exception if any validation fails.
     */
    static void validate_or_throw(const std::string& name, size_t size, const XSHMConfig& config = XSHMConfig{}) {
        if (!validate_name(name)) {
            throw XSHMException("Invalid shared memory name: " + name + 
                               " (must be 1-260 chars, alphanumeric + _-.)");
        }
        if (size > 0 && !validate_buffer_size(size, config)) {
            throw XSHMException("Invalid buffer size: " + std::to_string(size) + 
                               " (must be " + std::to_string(config.min_buffer_size) + 
                               "-" + std::to_string(config.max_buffer_size) + ")");
        }
    }
};

/**
 * @brief Shared memory header structure
 * 
 * This structure is placed at the beginning of shared memory and contains
 * metadata about the shared memory region, including buffer sizes, connection
 * state, and statistics.
 */
struct alignas(CACHE_LINE_SIZE) SharedMemoryHeader {
    /** @brief Magic number for integrity verification */
    std::atomic<uint32_t> magic_number;
    
    /** @brief Protocol version for compatibility checking */
    std::atomic<uint32_t> version;
    
    /** @brief Server-to-client buffer size */
    std::atomic<BufferSize> server_to_client_buffer_size;
    /** @brief Client-to-server buffer size */
    std::atomic<BufferSize> client_to_server_buffer_size;
    
    /** @brief Server connection state (0 = disconnected, 1 = connected) */
    std::atomic<uint32_t> server_connected{0};
    /** @brief Client connection state (0 = disconnected, 1 = connected) */
    std::atomic<uint32_t> client_connected{0};
    
    /** @brief Last server activity timestamp */
    std::atomic<Timestamp> last_server_activity{0};
    /** @brief Last client activity timestamp */
    std::atomic<Timestamp> last_client_activity{0};
    
    /** @brief Total messages sent via server-to-client channel */
    std::atomic<uint64_t> total_messages_sxc{0};
    /** @brief Total messages sent via client-to-server channel */
    std::atomic<uint64_t> total_messages_cxs{0};
    
    /** @brief Padding to align to cache line boundary */
    char padding[CACHE_LINE_SIZE - (sizeof(uint32_t) * 2 + sizeof(BufferSize) * 2 + 
                                   sizeof(std::atomic<uint32_t>) * 2 + 
                                   sizeof(std::atomic<Timestamp>) * 2 + 
                                   sizeof(std::atomic<uint64_t>) * 2) % CACHE_LINE_SIZE];
};

// Forward declarations
class UltimateSharedMemory;
template<typename T> class RingBuffer;
template<typename T> class DualRingBufferSystem;
template<typename T> class AsyncXSHM;

/**
 * @brief Event types for asynchronous callbacks
 * 
 * These events are triggered by the XSHM system and can be handled
 * by user-provided callback functions.
 */
enum class EventType {
    /** @brief Data received via server-to-client channel */
    DATA_RECEIVED_SXC,
    /** @brief Data received via client-to-server channel */
    DATA_RECEIVED_CXS,
    /** @brief Data sent via server-to-client channel */
    DATA_SENT_SXC,
    /** @brief Data sent via client-to-server channel */
    DATA_SENT_CXS,
    /** @brief Connection established between server and client */
    CONNECTION_ESTABLISHED,
    /** @brief Connection lost between server and client */
    CONNECTION_LOST,
    /** @brief Connection failed (handshake failed) */
    CONNECTION_FAILED
};

// Event callback function type
template<typename T>
using EventCallback = std::function<void(EventType, const T*, void*)>;

// RAII helper for handle cleanup
struct HandleGuard {
    HANDLE handle;
    HandleGuard(HANDLE h) : handle(h) {}
    ~HandleGuard() { 
        if (handle && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle); 
        }
    }
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    HandleGuard(HandleGuard&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
};

// Ultimate Shared Memory Manager
class UltimateSharedMemory {
private:
#ifdef _WIN32
    HANDLE hMapFile_;
    HandleGuard hMutex_;
    HandleGuard hEvent_;
#else
    int fd_;
    sem_t* sem_;
#endif
    void* ptr_;
    size_t size_;
    std::string name_;
    bool is_owner_;

public:
    explicit UltimateSharedMemory(const std::string& name, size_t size, bool create = false);
    ~UltimateSharedMemory();
    
    // Non-copyable, movable
    UltimateSharedMemory(const UltimateSharedMemory&) = delete;
    UltimateSharedMemory& operator=(const UltimateSharedMemory&) = delete;
    
    UltimateSharedMemory(UltimateSharedMemory&& other) noexcept;
    UltimateSharedMemory& operator=(UltimateSharedMemory&& other) noexcept;
    
    void* get() const noexcept { return ptr_; }
    size_t size() const noexcept { return size_; }
    const std::string& name() const noexcept { return name_; }
    bool is_owner() const noexcept { return is_owner_; }
    
    // Synchronization primitives
    void signal();
    void wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    static constexpr size_t next_power_of_2(size_t n) noexcept;
    
    // Friend class for access to next_power_of_2
    template<typename T>
    friend class DualRingBufferSystem;
};

// True Ring Buffer Implementation
/**
 * @brief Lock-free ring buffer for shared memory
 * 
 * This class implements a high-performance, lock-free ring buffer
 * designed for use in shared memory. It provides atomic operations
 * for reading and writing data without requiring mutexes.
 * 
 * @tparam T The type of data to store in the buffer
 * 
 * @note T must be trivially copyable and nothrow destructible
 * @note Buffer size must be a power of 2 for optimal performance
 * 
 * @example
 * ```cpp
 * // Create a ring buffer for integers
 * RingBuffer<int> buffer(data_ptr, 1024);
 * 
 * // Write data
 * int value = 42;
 * if (buffer.try_write(value)) {
 *     // Data written successfully
 * }
 * 
 * // Read data
 * int* data = buffer.try_read();
 * if (data) {
 *     // Process data
 *     buffer.commit_read();
 * }
 * ```
 */
template<typename T>
class RingBuffer {
    static_assert(RingBufferElement<T>::value, "T must be nothrow destructible and constructible");
    static_assert(TriviallySerializable<T>::value, "T must be trivially copyable");
    
private:
    // Atomic indices (fixed width for compatibility)
    alignas(CACHE_LINE_SIZE) std::atomic<BufferIndex> write_pos_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<BufferIndex> read_pos_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> read_sequence_{0};  // Sequence number for verification
    
    // Buffer size (power of 2 for efficiency)
    BufferSize capacity_;
    BufferSize mask_;  // capacity_ - 1 for fast modulo
    
    // Data pointer (in shared memory)
    T* data_;
    
    // Statistics
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> total_writes_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> total_reads_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> failed_writes_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> failed_reads_{0};
    
    // Helper method for CAS write operations
    template<typename WriteFunc>
    bool try_write_impl(WriteFunc&& write_func) noexcept {
        BufferIndex current_write = write_pos_.load(XSHM_RELAXED);
        BufferIndex next_write;
        BufferIndex current_read;
        int spins = 0;
        
        do {
            // Load read position with acquire to ensure we see latest consumer progress
            current_read = read_pos_.load(XSHM_ACQUIRE);
            next_write = (current_write + 1) & mask_;
            
            // Check if buffer is full
            if (next_write == current_read) {
                failed_writes_.fetch_add(1, XSHM_RELAXED);
                return false;
            }
            
            // Try to atomically update write position with release ordering
            // If CAS fails, current_write will be updated to actual value
            if (++spins > 16) {
                std::this_thread::yield(); // Yield CPU to prevent busy-wait
                spins = 0;
            }
        } while (!write_pos_.compare_exchange_weak(current_write, next_write, XSHM_RELEASE, XSHM_ACQUIRE));
        
        // Write data after successful CAS
        write_func(data_[current_write]);
        total_writes_.fetch_add(1, XSHM_RELAXED);
        
        return true;
    }
    
public:
    using value_type = T;
    using size_type = BufferSize;
    
    /**
     * @brief Default constructor for placement new
     * @note Used for placement new in shared memory
     */
    RingBuffer() noexcept = default;
    
    /**
     * @brief Constructor for placement in shared memory
     * @param memory Pointer to memory region for the buffer
     * @param capacity Buffer capacity (must be power of 2)
     * 
     * @note The memory region must be large enough to hold the buffer
     * @note Capacity will be rounded up to the next power of 2
     */
    RingBuffer(void* memory, BufferSize capacity);
    
    /**
     * @brief Destructor - no cleanup needed for shared memory buffers
     * 
     * @note Shared memory lifetime is managed externally, so no cleanup needed here.
     * The buffer data will be destroyed when the shared memory region is unmapped.
     * This prevents UB from concurrent access during destruction.
     */
    ~RingBuffer() noexcept = default;
    
    /**
     * @brief Write operations (lock-free)
     * @{
     */
    
    /**
     * @brief Try to write an item to the buffer (copy)
     * @param item The item to write
     * @return true if successful, false if buffer is full
     * 
     * This method attempts to write a copy of the item to the buffer.
     * It is thread-safe and lock-free.
     */
    bool try_write(const T& item) noexcept;
    
    /**
     * @brief Try to write an item to the buffer (move)
     * @param item The item to write (will be moved)
     * @return true if successful, false if buffer is full
     * 
     * This method attempts to move the item into the buffer.
     * It is thread-safe and lock-free.
     */
    bool try_write(T&& item) noexcept;
    
    /**
     * @brief Try to emplace an item in the buffer
     * @param args Arguments to construct the item
     * @return true if successful, false if buffer is full
     * 
     * This method constructs the item in-place using the provided arguments.
     * It is thread-safe and lock-free.
     */
    template<typename... Args>
    bool try_emplace(Args&&... args) noexcept;
    
    /** @} */
    
    /**
     * @brief Read operations (lock-free)
     * @{
     */
    
    /**
     * @brief Try to read an item from the buffer
     * @return Pointer to the item if available, nullptr if buffer is empty
     * 
     * This method returns a pointer to the next available item.
     * The caller must call commit_read() after processing the item.
     * It is thread-safe and lock-free.
     */
    T* try_read() noexcept;
    
    /**
     * @brief Try to read an item from the buffer with sequence verification
     * @param sequence Output parameter for sequence number
     * @return Pointer to the item if available, nullptr if buffer is empty
     * 
     * This method returns a pointer to the next available item with sequence verification.
     * The caller must call commit_read() with the sequence number for verification.
     * It is thread-safe and lock-free.
     */
    T* try_read(uint64_t& sequence) noexcept;
    
    /**
     * @brief Commit a read operation
     * 
     * This method must be called after processing an item obtained from try_read().
     * It advances the read position and makes the slot available for writing.
     * It is thread-safe and lock-free.
     */
    /**
     * @brief Commit a read operation
     * 
     * This method must be called after processing an item obtained from try_read().
     * It advances the read position and makes the slot available for writing.
     * It is thread-safe and lock-free.
     */
    
    /**
     * @brief Commit a read operation with sequence verification
     * @param expected_sequence The sequence number from try_read()
     * @return true if commit was successful, false if sequence mismatch
     * 
     * This method must be called after processing an item obtained from try_read().
     * It advances the read position atomically with sequence verification.
     * It is thread-safe and lock-free.
     */
    bool commit_read(uint64_t expected_sequence) noexcept;
    
    /**
     * @brief Commit a read operation without sequence verification (legacy)
     * @return true if successful, false if no data to commit
     * 
     * This method commits the current read position without sequence verification.
     * Use only when sequence verification is not needed.
     * It is thread-safe and lock-free.
     */
    bool commit_read() noexcept;
    
    /**
     * @brief Initialize buffer for shared memory
     * @param buffer_size Size of the buffer
     * 
     * This method initializes the buffer for use in shared memory.
     * It should only be called during buffer creation.
     */
    void initialize_for_shared_memory(BufferSize buffer_size) noexcept;
    
    /**
     * @brief Set buffer capacity and mask
     * @param capacity Buffer capacity (must be power of 2)
     * 
     * This method sets the capacity and mask for the buffer.
     * Used internally for shared memory initialization.
     */
    void set_capacity(BufferSize capacity) noexcept;
    
    /** @} */
    
    /**
     * @brief Buffer status
     * @{
     */
    
    /**
     * @brief Check if buffer is empty
     * @return true if buffer is empty, false otherwise
     */
    XSHM_NODISCARD bool empty() const noexcept;
    
    /**
     * @brief Check if buffer is full
     * @return true if buffer is full, false otherwise
     */
    XSHM_NODISCARD bool full() const noexcept;
    
    /**
     * @brief Get current number of items in buffer
     * @return Number of items currently in buffer
     */
    XSHM_NODISCARD BufferSize size() const noexcept;
    
    /**
     * @brief Get buffer capacity
     * @return Maximum number of items the buffer can hold
     */
    XSHM_NODISCARD BufferSize capacity() const noexcept;
    
    /** @} */
    
    /**
     * @brief Statistics
     * @{
     */
    
    /**
     * @brief Get total number of successful writes
     * @return Number of successful write operations
     */
    XSHM_NODISCARD uint64_t total_writes() const noexcept;
    
    /**
     * @brief Get total number of successful reads
     * @return Number of successful read operations
     */
    XSHM_NODISCARD uint64_t total_reads() const noexcept;
    
    /**
     * @brief Get total number of failed writes
     * @return Number of failed write operations (buffer full)
     */
    XSHM_NODISCARD uint64_t failed_writes() const noexcept;
    
    /**
     * @brief Get total number of failed reads
     * @return Number of failed read operations (buffer empty)
     */
    XSHM_NODISCARD uint64_t failed_reads() const noexcept;
    
    /**
     * @brief Reset all statistics to zero
     */
    void reset_statistics() noexcept;
    
    /**
     * @brief Set data pointer (for shared memory initialization)
     * @param data_ptr Pointer to data array
     */
    void set_data_pointer(T* data_ptr) noexcept;
    
    /** @} */

private:
    // Helper functions
    static constexpr BufferSize next_power_of_2(BufferSize n) noexcept;
    BufferIndex next_write_pos() const noexcept;
    BufferIndex next_read_pos() const noexcept;
};

// Dual Ring Buffer System
/**
 * @brief Dual Ring Buffer System for bidirectional communication
 * 
 * This class manages two ring buffers for bidirectional communication
 * between a server and client process. It provides a complete solution
 * for shared memory-based IPC with automatic synchronization.
 * 
 * @tparam T The type of data to communicate
 * 
 * @note T must be trivially copyable and nothrow destructible
 * @note This class is not copyable or movable
 * 
 * @example
 * ```cpp
 * // Server side
 * DualRingBufferSystem<MyData> server("my_app", 1024, true);
 * 
 * // Send data to client
 * MyData data;
 * if (server.server_to_client().try_write(data)) {
 *     server.signal_server_to_client();
 * }
 * 
 * // Receive data from client
 * MyData* received = server.server_from_client().try_read();
 * if (received) {
 *     // Process data
 *     server.server_from_client().commit_read();
 * }
 * ```
 */
template<typename T>
class DualRingBufferSystem {
private:
    /**
     * @brief Calculate proper alignment for atomic types
     * @tparam U Type to align
     * @return Proper alignment size (power of 2)
     */
    template<typename U>
    static constexpr size_t calculate_alignment() {
        constexpr size_t atomic_size = sizeof(std::atomic<U>);
        constexpr size_t cache_line = CACHE_LINE_SIZE;
        // Ensure result is power of 2
        return (atomic_size > cache_line) ? atomic_size : cache_line;
    }
    
    struct SharedLayout {
        alignas(CACHE_LINE_SIZE) SharedMemoryHeader header;
        alignas(CACHE_LINE_SIZE) RingBuffer<T> server_to_client_buffer;
        alignas(CACHE_LINE_SIZE) RingBuffer<T> client_to_server_buffer;
        alignas(CACHE_LINE_SIZE) char server_to_client_data[1];  // Dynamic size
        alignas(CACHE_LINE_SIZE) char client_to_server_data[1];  // Dynamic size
    };
    
    /**
     * @brief TOCTOU protection utilities for XSHM
     * 
     * This class provides protection against Time-of-Check-Time-of-Use attacks
     * by implementing atomic validation and integrity checks.
     */
    class XSHMTOCTOUProtection {
    public:
        /**
         * @brief Validates shared memory integrity with TOCTOU protection
         * @param layout Pointer to shared memory layout
         * @param config Configuration with security settings
         * @return true if validation passes, false otherwise
         * 
         * This method performs atomic validation of shared memory integrity,
         * preventing TOCTOU attacks by checking all critical fields atomically.
         */
        static bool validate_integrity_atomic(const SharedLayout* layout, const XSHMConfig& config) {
            if (!layout || !config.enable_toctou_protection) {
                return true; // Skip validation if disabled
            }
            
            // Atomic read of critical fields
            const uint32_t magic = layout->header.magic_number.load(XSHM_ACQUIRE);
            const uint32_t version = layout->header.version.load(XSHM_ACQUIRE);
            const BufferSize sxc_size = layout->header.server_to_client_buffer_size.load(XSHM_ACQUIRE);
            const BufferSize cxs_size = layout->header.client_to_server_buffer_size.load(XSHM_ACQUIRE);
            
            // Validate magic number
            if (config.enable_integrity_checks && magic != MAGIC_NUMBER) {
                return false;
            }
            
            // Validate protocol version
            if (config.enable_version_validation && version != PROTOCOL_VERSION) {
                return false;
            }
            
            // Validate buffer sizes are consistent
            if (sxc_size != cxs_size) {
                return false;
            }
            
            // Validate buffer sizes are within limits
            if (sxc_size < config.min_buffer_size || sxc_size > config.max_buffer_size) {
                return false;
            }
            
            // Validate connection status atomically
            const auto server_connected = layout->header.server_connected.load(XSHM_ACQUIRE);
            const auto client_connected = layout->header.client_connected.load(XSHM_ACQUIRE);
            
            // Check for reasonable connection states
            if (server_connected > 1 || client_connected > 1) {
                return false;
            }
            
            return true;
        }
        
        /**
         * @brief Performs retry-based validation with exponential backoff
         * @param layout Pointer to shared memory layout
         * @param config Configuration with security settings
         * @return true if validation passes after retries, false otherwise
         * 
         * This method implements retry logic with exponential backoff to handle
         * transient TOCTOU conditions.
         */
        static bool validate_with_retry(const SharedLayout* layout, const XSHMConfig& config) {
            if (!config.enable_toctou_protection) {
                return true;
            }
            
            for (size_t attempt = 0; attempt < config.max_validation_retries; ++attempt) {
                if (validate_integrity_atomic(layout, config)) {
                    return true;
                }
                
                // Exponential backoff: 1ms, 2ms, 4ms, 8ms...
                if (attempt < config.max_validation_retries - 1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1 << attempt));
                }
            }
            
            return false;
        }
    };
    
    std::unique_ptr<UltimateSharedMemory> shm_;
    SharedLayout* layout_;
    RingBuffer<T>* server_to_client_buffer_;
    RingBuffer<T>* client_to_server_buffer_;
    
    // Synchronization objects with RAII cleanup
    HandleGuard hMutex_;
    HandleGuard hEventServerToClient_;
    HandleGuard hEventClientToServer_;
    HandleGuard hEventConnection_;
    
    
    std::string name_;
    bool is_server_;

public:
    /**
     * @brief Constructor for server (creates shared memory)
     * @param name Unique name for the shared memory region
     * @param buffer_size Size of each ring buffer (will be rounded to power of 2)
     * @param is_server Whether this is a server instance (default: true)
     * @param config Configuration with customizable limits
     * 
     * This constructor creates a new shared memory region and initializes
     * the dual ring buffer system. It is typically used by the server process.
     * 
     * @throws XSHMException if shared memory creation fails
     * @note The name must be unique across all processes
     * @note Buffer size will be validated against config limits
     */
    DualRingBufferSystem(std::string name, BufferSize buffer_size, bool is_server = true, const XSHMConfig& config = XSHMConfig{});
    
    /**
     * @brief Constructor for client (connects to existing memory)
     * @param name Name of the existing shared memory region
     * 
     * This constructor connects to an existing shared memory region
     * created by a server process. It is typically used by client processes.
     * 
     * @throws XSHMException if connection to shared memory fails
     * @note The server must be running before the client connects
     */
    
    ~DualRingBufferSystem();
    
    // Non-copyable, non-movable
    DualRingBufferSystem(const DualRingBufferSystem&) = delete;
    DualRingBufferSystem& operator=(const DualRingBufferSystem&) = delete;
    DualRingBufferSystem(DualRingBufferSystem&&) = delete;
    DualRingBufferSystem& operator=(DualRingBufferSystem&&) = delete;
    
    // Interface for server
    RingBuffer<T>& server_to_client() noexcept { return *server_to_client_buffer_; }
    RingBuffer<T>& server_from_client() noexcept { return *client_to_server_buffer_; }
    
    // Interface for client
    RingBuffer<T>& client_to_server() noexcept { return *client_to_server_buffer_; }
    RingBuffer<T>& client_from_server() noexcept { return *server_to_client_buffer_; }
    
    // Synchronization - only non-blocking signals
    void signal_server_to_client() noexcept;  // Signal: data in server-to-client
    void signal_client_to_server() noexcept;  // Signal: data in client-to-server
    void signal_connection() noexcept;  // Signal: connection change
    
    // Get events for WaitForMultipleObjects
    HANDLE get_server_to_client_event() const noexcept { return hEventServerToClient_.handle; }
    HANDLE get_client_to_server_event() const noexcept { return hEventClientToServer_.handle; }
    HANDLE get_connection_event() const noexcept { return hEventConnection_.handle; }
    
    /**
     * @brief Update server activity timestamp
     * @param timestamp Current timestamp
     */
    void update_server_activity(uint64_t timestamp) noexcept {
        if (layout_) {
            layout_->header.last_server_activity.store(timestamp, XSHM_RELEASE);
        }
    }
    
    /**
     * @brief Update client activity timestamp
     * @param timestamp Current timestamp
     */
    void update_client_activity(uint64_t timestamp) noexcept {
        if (layout_) {
            layout_->header.last_client_activity.store(timestamp, XSHM_RELEASE);
        }
    }
    
    // Statistics
    struct Statistics {
        uint64_t server_to_client_writes, server_to_client_reads, server_to_client_failed_writes, server_to_client_failed_reads;
        uint64_t client_to_server_writes, client_to_server_reads, client_to_server_failed_writes, client_to_server_failed_reads;
    };
    XSHM_NODISCARD Statistics get_statistics() const noexcept;
    void reset_statistics() noexcept;
    
    // Connection check
    XSHM_NODISCARD bool is_server_connected() const noexcept;
    XSHM_NODISCARD bool is_client_connected() const noexcept;
    XSHM_NODISCARD bool is_connected() const noexcept;
};

// ============================================================================
// ============================================================================
// SINGLE ASYNC API
// ============================================================================

/**
 * @brief High-level asynchronous API for XSHM
 * 
 * This class provides a simplified, asynchronous interface for XSHM
 * communication. It handles all the complexity of shared memory management,
 * event loops, and synchronization automatically.
 * 
 * @tparam T The type of data to communicate
 * 
 * @note T must be trivially copyable and nothrow destructible
 * @note This class is not copyable or movable
 * 
 * @example
 * ```cpp
 * // Server side
 * auto server = AsyncXSHM<MyData>::create_server("my_app", 1024);
 * 
 * // Set up event handlers
 * recv_cxs(server, [](const MyData* data) {
 *     if (data) {
 *         std::cout << "Received: " << data->message << std::endl;
 *     }
 * });
 * 
 * // Send data
 * MyData data;
 * send_sxc(server, data);
 * 
 * // Client side
 * auto client = AsyncXSHM<MyData>::connect("my_app");
 * 
 * // Set up event handlers
 * recv_sxc(client, [](const MyData* data) {
 *     if (data) {
 *         std::cout << "Received: " << data->message << std::endl;
 *     }
 * });
 * 
 * // Send data
 * MyData data;
 * send_cxs(client, data);
 * ```
 */
template<typename T>
class AsyncXSHM {
private:
    std::unique_ptr<DualRingBufferSystem<T>> buffers_;
    std::vector<EventCallback<T>> callbacks_;
    std::recursive_mutex callbacks_mutex_;
    std::atomic<bool> running_{false};
    std::thread event_thread_;
    std::string name_;
    bool is_server_{false};
    XSHMConfig config_;
    
    // Event synchronization
    HANDLE stop_event_{INVALID_HANDLE_VALUE};
    
    // Performance optimization - Thread pool for async callbacks
    std::vector<std::thread> callback_threads_;
    std::queue<std::function<void()>> callback_queue_;
    std::mutex callback_queue_mutex_;
    std::condition_variable callback_queue_cv_;
    std::atomic<bool> callback_threads_running_{false};
    
    // Thread-safe logging
    mutable std::mutex logging_mutex_;
    
    // Reconnect protection
    std::atomic<bool> reconnecting_{false};
    
    // Helper function to update activity timestamp
    void update_activity_timestamp() const {
        if (buffers_) {
            const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            
            // Update activity timestamp through DualRingBufferSystem interface
            if (is_server_) {
                buffers_->update_server_activity(now);
            } else {
                buffers_->update_client_activity(now);
            }
        }
    }

public:
    /**
     * @brief Create a server instance
     * @param name Unique name for the shared memory region
     * @param buffer_size Size of each ring buffer (default: 16384)
     * @param config Configuration with customizable limits
     * @return Unique pointer to the server instance
     * 
     * This method creates a new server instance that manages a shared memory
     * region for communication with client processes. The server automatically
     * starts its event loop and is ready to accept connections.
     * 
     * @throws XSHMException if server creation fails
     * @note The name must be unique across all processes
     * @note Buffer size will be rounded up to the next power of 2
     * @note Configuration allows customizing limits and timeouts
     * 
     * @example
     * ```cpp
     * // Default configuration
     * auto server = AsyncXSHM<MyData>::create_server("my_app", 1024);
     * 
     * // High-performance configuration for server
     * XSHMConfig server_config;
     * server_config.max_buffer_size = 2 * 1024 * 1024; // 2MB
     * server_config.event_loop_timeout_ms = 100;       // Faster polling
     * server_config.enable_statistics = true;
     * server_config.enable_batch_processing = true;   // Process multiple messages
     * server_config.max_batch_size = 64;               // Up to 64 messages per batch
     * server_config.enable_async_callbacks = true;     // Non-blocking callbacks
     * server_config.callback_thread_pool_size = 8;     // 8 threads for callbacks
     * server_config.max_callback_timeout_ms = 5;       // 5ms max per batch
     * auto server = AsyncXSHM<MyData>::create_server("my_app", 1024, server_config);
     * 
     * // High-performance configuration for client
     * XSHMConfig client_config;
     * client_config.event_loop_timeout_ms = 50;        // Very fast polling
     * client_config.enable_auto_reconnect = true;
     * client_config.max_retry_attempts = 5;
     * client_config.enable_batch_processing = true;   // Batch processing
     * client_config.max_batch_size = 32;               // Smaller batches for client
     * client_config.enable_async_callbacks = true;     // Async callbacks
     * client_config.callback_thread_pool_size = 4;     // 4 threads for callbacks
     * auto client = AsyncXSHM<MyData>::connect("my_app", client_config);
     * ```
     */
    static std::unique_ptr<AsyncXSHM<T>> create_server(std::string name, BufferSize buffer_size = 16384, const XSHMConfig& config = XSHMConfig{}) {
        // Validate input parameters
        XSHMValidator::validate_or_throw(name, buffer_size, config);
        
        auto instance = std::unique_ptr<AsyncXSHM<T>>(new AsyncXSHM<T>());
        instance->name_ = std::move(name);  // MOVE instead of copy
        instance->is_server_ = true;
        instance->config_ = config;
        instance->stop_event_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!instance->stop_event_) {
            throw XSHMException("Failed to create stop event");
        }
        instance->buffers_ = std::make_unique<DualRingBufferSystem<T>>(instance->name_, buffer_size, true, config);
        instance->start_event_loop();
        return instance;
    }
    
    /**
     * @brief Connect to an existing server
     * @param name Name of the shared memory region created by the server
     * @param config Configuration for client-specific settings (optional)
     * @return Unique pointer to the client instance
     * 
     * This method connects to an existing server instance by opening
     * the shared memory region. The client automatically starts its
     * event loop and sends a connection notification to the server.
     * 
     * @throws XSHMException if connection fails
     * @note The server must be running before calling this method
     * @note The client will automatically notify the server of its connection
     * @note Client can only configure timeouts, statistics, and reconnection settings
     * @note Buffer size settings in config are ignored for clients
     * 
     * @example
     * ```cpp
     * // Default configuration
     * auto client = AsyncXSHM<MyData>::connect("my_app");
     * 
     * // Custom configuration for client
     * XSHMConfig config;
     * config.event_loop_timeout_ms = 500;
     * config.enable_auto_reconnect = true;
     * config.max_retry_attempts = 5;
     * auto client = AsyncXSHM<MyData>::connect("my_app", config);
     * ```
     */
    static std::unique_ptr<AsyncXSHM<T>> connect(std::string name, const XSHMConfig& config = XSHMConfig{}) {
        // Validate input parameters (size = 0 means no size validation for client)
        XSHMValidator::validate_or_throw(name, 0, config);
        
        auto instance = std::unique_ptr<AsyncXSHM<T>>(new AsyncXSHM<T>());
        instance->name_ = std::move(name);  // MOVE instead of copy
        instance->is_server_ = false;
        instance->config_ = config;
        instance->stop_event_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!instance->stop_event_) {
            throw XSHMException("Failed to create stop event");
        }
        instance->buffers_ = std::make_unique<DualRingBufferSystem<T>>(instance->name_, 0, false, config);
        instance->start_event_loop();
        
        // Send connection notification with retry logic to avoid race conditions
        bool connection_success = false;
        try {
            // Retry send with adaptive timeout to handle buffer full scenarios
            for (size_t attempt = 0; attempt < config.max_retry_attempts; ++attempt) {
                // Create new notification for each attempt to avoid move issues
            T connection_notification{};  // Initialize with default constructor
                
                auto send_future = instance->send_cxs(std::move(connection_notification));
                if (send_future.get()) {
                    connection_success = true;
                    break;
                }
                
                // Log retry attempt if logging enabled
                if (config.enable_logging) {
                    std::lock_guard<std::mutex> lock(instance->logging_mutex_);
                    std::cerr << "XSHM: Connect retry attempt " << (attempt + 1) << "/" << config.max_retry_attempts << " failed" << std::endl;
                }
                
                // Adaptive backoff: exponential delay with config timeout
                if (attempt < config.max_retry_attempts - 1) {
                    const size_t exponential_delay = config.initial_retry_delay_ms * (1 << attempt);
                    const size_t max_delay = config.connection_timeout_ms / config.max_retry_attempts;
                    const size_t delay_ms = (exponential_delay < max_delay) ? exponential_delay : max_delay;
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                }
            }
            
            // Log final result if logging enabled
            if (config.enable_logging) {
                std::lock_guard<std::mutex> lock(instance->logging_mutex_);
                if (connection_success) {
                    std::cerr << "XSHM: Connection established successfully" << std::endl;
                } else {
                    std::cerr << "XSHM: Connect retry failed after " << config.max_retry_attempts << " attempts" << std::endl;
                }
            }
            
            // Trigger appropriate event based on success
            if (connection_success) {
            instance->trigger_event(EventType::CONNECTION_ESTABLISHED, nullptr);
            } else {
                instance->trigger_event(EventType::CONNECTION_FAILED, nullptr);
            }
            
        } catch (const std::exception&) {
            // Trigger failed event on exception
            instance->trigger_event(EventType::CONNECTION_FAILED, nullptr);
        }
        
        return instance;
    }
    
    ~AsyncXSHM() {
        stop_event_loop();
        if (stop_event_) {
            CloseHandle(stop_event_);
        }
    }
    
    // Non-copyable, non-movable
    AsyncXSHM(const AsyncXSHM&) = delete;
    AsyncXSHM& operator=(const AsyncXSHM&) = delete;
    AsyncXSHM(AsyncXSHM&&) = delete;
    AsyncXSHM& operator=(AsyncXSHM&&) = delete;
    
    // Event system
    void on_data_received_sxc(std::function<void(const T*)> callback) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_mutex_);
        callbacks_.push_back([callback](EventType type, const T* data, void*) {
            if (type == EventType::DATA_RECEIVED_SXC) {
                callback(data);
            }
        });
    }
    
    void on_data_received_cxs(std::function<void(const T*)> callback) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_mutex_);
        callbacks_.push_back([callback](EventType type, const T* data, void*) {
            if (type == EventType::DATA_RECEIVED_CXS) {
                callback(data);
            }
        });
    }
    
    void on_data_sent_sxc(std::function<void(const T*)> callback) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_mutex_);
        callbacks_.push_back([callback](EventType type, const T* data, void*) {
            if (type == EventType::DATA_SENT_SXC) {
                callback(data);
            }
        });
    }
    
    void on_data_sent_cxs(std::function<void(const T*)> callback) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_mutex_);
        callbacks_.push_back([callback](EventType type, const T* data, void*) {
            if (type == EventType::DATA_SENT_CXS) {
                callback(data);
            }
        });
    }
    
    void on_connection_established(std::function<void()> callback) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_mutex_);
        callbacks_.push_back([callback](EventType type, const T*, void*) {
            if (type == EventType::CONNECTION_ESTABLISHED) {
                callback();
            }
        });
    }
    
    void on_connection_failed(std::function<void()> callback) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_mutex_);
        callbacks_.push_back([callback](EventType type, const T*, void*) {
            if (type == EventType::CONNECTION_FAILED) {
                callback();
            }
        });
    }
    
    
    // FULLY ASYNC SEND OPERATIONS - MOVE only for maximum performance
    std::future<bool> send_sxc(T data) {
        return std::async(std::launch::async, [this, data = std::move(data)]() mutable {
            // Copy data before move to avoid dangling pointer
            T data_copy = data;
            bool success = buffers_->server_to_client().try_write(std::move(data));
            if (success) {
                buffers_->signal_server_to_client();
                update_activity_timestamp();
                trigger_event(EventType::DATA_SENT_SXC, &data_copy);
            }
            return success;
        });
    }
    
    std::future<bool> send_cxs(T data) {
        return std::async(std::launch::async, [this, data = std::move(data)]() mutable {
            // Copy data before move to avoid dangling pointer
            T data_copy = data;
            bool success = buffers_->client_to_server().try_write(std::move(data));
            if (success) {
                buffers_->signal_client_to_server();
                update_activity_timestamp();
                trigger_event(EventType::DATA_SENT_CXS, &data_copy);
            }
            return success;
        });
    }
    
    // COPY versions for cases when original needs to be preserved (also async!)
    std::future<bool> send_sxc_copy(const T& data) {
        return std::async(std::launch::async, [this, data]() {
            bool success = buffers_->server_to_client().try_write(data);
            if (success) {
                buffers_->signal_server_to_client();
                trigger_event(EventType::DATA_SENT_SXC, &data);
            }
            return success;
        });
    }
    
    std::future<bool> send_cxs_copy(const T& data) {
        return std::async(std::launch::async, [this, data]() {
            bool success = buffers_->client_to_server().try_write(data);
            if (success) {
                buffers_->signal_client_to_server();
                trigger_event(EventType::DATA_SENT_CXS, &data);
            }
            return success;
        });
    }
    
    // New methods for macros (avoid conflicts)
    std::future<bool> send_to_client(T data) {
        return send_sxc(std::move(data));
    }
    
    std::future<bool> send_to_server(T data) {
        return send_cxs(std::move(data));
    }
    
    std::future<bool> send_to_client_copy(const T& data) {
        return send_sxc_copy(data);
    }
    
    std::future<bool> send_to_server_copy(const T& data) {
        return send_cxs_copy(data);
    }
    
    // Status
    XSHM_NODISCARD bool is_connected() const noexcept {
        if (is_server_) {
            // Server is considered connected if client is connected
            return buffers_->is_client_connected();
        } else {
            // Client is considered connected if server is connected
            return buffers_->is_server_connected();
        }
    }
    
    XSHM_NODISCARD bool is_server() const noexcept {
        return is_server_;
    }
    
    XSHM_NODISCARD bool is_client() const noexcept {
        return !is_server_;
    }
    
    // Statistics
    XSHM_NODISCARD typename DualRingBufferSystem<T>::Statistics get_statistics() const noexcept {
        return buffers_->get_statistics();
    }
    
    void reset_statistics() noexcept {
        buffers_->reset_statistics();
    }
    
    /**
     * @brief Configuration management
     * @{
     */
    
    /**
     * @brief Get current configuration
     * @return Reference to the current configuration
     */
    const XSHMConfig& get_config() const noexcept {
        return config_;
    }
    
    /**
     * @brief Update configuration (requires restart for some settings)
     * @param new_config New configuration to apply
     * @return true if configuration was updated, false if invalid
     * 
     * @note Some configuration changes require restarting the event loop
     * @note Buffer size changes require recreating the instance
     */
    bool update_config(const XSHMConfig& new_config) noexcept {
        if (!new_config.is_valid()) {
            return false;
        }
        
        // Update timeout settings immediately
        if (new_config.event_loop_timeout_ms != config_.event_loop_timeout_ms) {
            config_.event_loop_timeout_ms = new_config.event_loop_timeout_ms;
        }
        
        // Update other settings
        config_.connection_timeout_ms = new_config.connection_timeout_ms;
        config_.enable_statistics = new_config.enable_statistics;
        config_.enable_auto_reconnect = new_config.enable_auto_reconnect;
        config_.max_retry_attempts = new_config.max_retry_attempts;
        
        return true;
    }
    
    /** @} */
    
    /**
     * @brief Performance optimization methods
     * @{
     */
    
    /**
     * @brief Start callback thread pool for async processing
     * 
     * This method starts a thread pool to handle callbacks asynchronously,
     * preventing the event loop from being blocked by slow callbacks.
     */
    void start_callback_thread_pool() {
        if (callback_threads_running_.load()) {
            return; // Already running
        }
        
        callback_threads_running_.store(true);
        callback_threads_.reserve(config_.callback_thread_pool_size);
        
        for (size_t i = 0; i < config_.callback_thread_pool_size; ++i) {
            callback_threads_.emplace_back([this]() {
                while (callback_threads_running_.load()) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(callback_queue_mutex_);
                        callback_queue_cv_.wait(lock, [this]() {
                            return !callback_queue_.empty() || !callback_threads_running_.load();
                        });
                        
                        if (!callback_threads_running_.load()) {
                            break;
                        }
                        
                        task = std::move(callback_queue_.front());
                        callback_queue_.pop();
                    }
                    
                    try {
                        task();
                    } catch (const std::exception&) {
                        // Ignore errors in callbacks
                    }
                }
            });
        }
    }
    
    /**
     * @brief Stop callback thread pool
     * 
     * This method stops the callback thread pool and waits for all
     * pending callbacks to complete.
     */
    void stop_callback_thread_pool() {
        callback_threads_running_.store(false);
        callback_queue_cv_.notify_all();
        
        for (auto& thread : callback_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        callback_threads_.clear();
    }
    
    /**
     * @brief Submit callback task to thread pool
     * @param task The callback task to execute
     * 
     * This method submits a callback task to the thread pool for
     * asynchronous execution, preventing blocking of the event loop.
     */
    void submit_callback_task(std::function<void()> task) {
        if (!config_.enable_async_callbacks) {
            // Execute synchronously if async callbacks are disabled
            task();
            return;
        }
        
        {
            std::lock_guard<std::mutex> lock(callback_queue_mutex_);
            callback_queue_.push(std::move(task));
        }
        callback_queue_cv_.notify_one();
    }
    
    /** @} */
    
    // Event loop management
    void start_event_loop() {
        if (running_.load()) {
            return; // Already running
        }
        running_.store(true);
        
        // Start callback thread pool for async processing
        start_callback_thread_pool();
        
        event_thread_ = std::thread([this]() {
            static bool was_connected = false;
            
            while (running_.load()) {
                try {
                    // Prepare handles for WaitForMultipleObjects
                    HANDLE handles[4] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
                    DWORD handle_count = 0;
                    
                    // Add stop event
                    handles[handle_count++] = stop_event_;
                    
                    // Add buffer events if available and valid
                    if (buffers_) {
                        HANDLE sxc_event = buffers_->get_server_to_client_event();
                        HANDLE cxs_event = buffers_->get_client_to_server_event();
                        HANDLE conn_event = buffers_->get_connection_event();
                        
                        if (sxc_event != INVALID_HANDLE_VALUE) {
                            handles[handle_count++] = sxc_event;
                        }
                        if (cxs_event != INVALID_HANDLE_VALUE) {
                            handles[handle_count++] = cxs_event;
                        }
                        if (conn_event != INVALID_HANDLE_VALUE) {
                            handles[handle_count++] = conn_event;
                        }
                    }
                    
                    // Wait for events (with configurable timeout for periodic checks)
                    DWORD result = WaitForMultipleObjects(handle_count, handles, FALSE, static_cast<DWORD>(config_.event_loop_timeout_ms));
                    
                    if (result == WAIT_OBJECT_0) {
                        // Stop event signaled
                        break;
                    } else if (result == WAIT_OBJECT_0 + 1) {
                        // Server-to-client data event
                        process_server_to_client_data();
                    } else if (result == WAIT_OBJECT_0 + 2) {
                        // Client-to-server data event
                        process_client_to_server_data();
                    } else if (result == WAIT_OBJECT_0 + 3) {
                        // Connection event
                        process_connection_event(was_connected);
                    } else if (result == WAIT_TIMEOUT) {
                        // Timeout - periodic check for connection status
                        process_connection_status(was_connected);
                    }
                    
                } catch (const std::exception&) {
                    // Ignore errors in event loop
                }
            }
        });
    }
    
    void stop_event_loop() {
        running_.store(false);
        if (stop_event_) {
            SetEvent(stop_event_);
        }
        if (event_thread_.joinable()) {
            event_thread_.join();
        }
        
        // Stop callback thread pool
        stop_callback_thread_pool();
    }
    
private:
    void process_server_to_client_data() {
        if (!buffers_) return;
        
        if (config_.enable_batch_processing) {
            process_batch_data(EventType::DATA_RECEIVED_SXC, 
                             [this]() -> std::pair<T*, uint64_t> { 
                                 uint64_t sequence;
                                 T* data = buffers_->server_to_client().try_read(sequence);
                                 return {data, sequence};
                             },
                             [this](uint64_t sequence) { 
                                 buffers_->server_to_client().commit_read(sequence);
                             });
        } else {
            // Single message processing - always use async callbacks to avoid blocking
            uint64_t sequence = 0;
            T* data = buffers_->server_to_client().try_read(sequence);
            if (data) {
                // Copy data to avoid dangling pointer issues
                T data_copy = *data;
                buffers_->server_to_client().commit_read(sequence);
                
                // Update activity timestamp on successful read
                update_activity_timestamp();
                
                // Submit to thread pool for async execution
                submit_callback_task([this, data_copy]() {
                    trigger_event(EventType::DATA_RECEIVED_SXC, &data_copy);
                });
            }
        }
    }
    
    void process_client_to_server_data() {
        if (!buffers_) return;
        
        if (config_.enable_batch_processing) {
            process_batch_data(EventType::DATA_RECEIVED_CXS, 
                             [this]() -> std::pair<T*, uint64_t> { 
                                 uint64_t sequence;
                                 T* data = buffers_->client_to_server().try_read(sequence);
                                 return {data, sequence};
                             },
                             [this](uint64_t sequence) { 
                                 buffers_->client_to_server().commit_read(sequence);
                             });
        } else {
            // Single message processing - always use async callbacks to avoid blocking
            uint64_t sequence = 0;
            T* data = buffers_->client_to_server().try_read(sequence);
            if (data) {
                // Copy data to avoid dangling pointer issues
                T data_copy = *data;
                buffers_->client_to_server().commit_read(sequence);
                
                // Update activity timestamp on successful read
                update_activity_timestamp();
                
                // Submit to thread pool for async execution
                submit_callback_task([this, data_copy]() {
                    trigger_event(EventType::DATA_RECEIVED_CXS, &data_copy);
                });
            }
        }
    }
    
    /**
     * @brief Process multiple messages in batch for better performance
     * @param event_type The event type to trigger
     * @param try_read_func Function to try reading data
     * @param commit_read_func Function to commit read operation
     */
    void process_batch_data(EventType event_type, 
                           std::function<std::pair<T*, uint64_t>()> try_read_func,
                           std::function<void(uint64_t)> commit_read_func) {
        const auto start_time = std::chrono::steady_clock::now();
        size_t processed_count = 0;
        
        // Process up to max_batch_size messages or until timeout
        while (processed_count < config_.max_batch_size) {
            // Check timeout
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= config_.max_callback_timeout_ms) {
                break;
            }
            
            auto [data, sequence] = try_read_func();
            if (!data) {
                break; // No more data available
            }
            
            // Copy data to avoid dangling pointer issues
            T data_copy = *data;
            commit_read_func(sequence);
            
            // Submit callback task to thread pool for async execution
            submit_callback_task([this, event_type, data_copy]() {
                trigger_event(event_type, &data_copy);
            });
            
            ++processed_count;
        }
    }
    
    void process_connection_event(bool& was_connected) {
        // Connection event was signaled - check status
        process_connection_status(was_connected);
    }
    
    void process_connection_status(bool& was_connected) {
                    bool is_connected = this->is_connected();
                    if (is_connected && !was_connected) {
            // Submit connection established event to thread pool
            submit_callback_task([this]() {
                        trigger_event(EventType::CONNECTION_ESTABLISHED, nullptr);
            });
                        
            // If this is server, send ready notification to client
                        if (is_server_) {
                submit_callback_task([this]() {
                                T ready_notification;
                                std::memset(&ready_notification, 0, sizeof(T));
                    this->send_sxc(std::move(ready_notification));
                });
                        }
                    } else if (!is_connected && was_connected) {
            // Submit connection lost event to thread pool
            submit_callback_task([this]() {
                        trigger_event(EventType::CONNECTION_LOST, nullptr);
            });
            
            // Auto-reconnect if enabled
            if (config_.enable_auto_reconnect) {
                // Check if already reconnecting to prevent race conditions
                bool expected = false;
                if (!reconnecting_.compare_exchange_strong(expected, true)) {
                    return; // Already reconnecting
                }
                
                submit_callback_task([this]() {
                    bool reconnect_success = false;
                    
                    // Retry loop with backoff
                    for (size_t attempt = 0; attempt < config_.max_retry_attempts; ++attempt) {
                        try {
                            // Stop event loop before reconnect to prevent race condition
                            stop_event_loop();
                            
                            // Attempt to reconnect
                            auto new_buffers = std::make_unique<DualRingBufferSystem<T>>(name_, 0, false, config_);
                            buffers_ = std::move(new_buffers);
                            reconnect_success = true;
                            
                            // Restart event loop after successful reconnect
                            start_event_loop();
                            
                            if (config_.enable_logging) {
                                std::lock_guard<std::mutex> lock(logging_mutex_);
                                std::cerr << "XSHM: Auto-reconnect successful on attempt " << (attempt + 1) << std::endl;
                            }
                            break;
                            
                } catch (const std::exception& e) {
                            if (config_.enable_logging) {
                                std::lock_guard<std::mutex> lock(logging_mutex_);
                                std::cerr << "XSHM: Auto-reconnect attempt " << (attempt + 1) << " failed: " << e.what() << std::endl;
                            }
                            
                            // Backoff before retry
                            if (attempt < config_.max_retry_attempts - 1) {
                                const size_t delay_ms = config_.initial_retry_delay_ms * (1 << attempt);
                                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                            }
                        }
                    }
                    
                    // Log final result
                    if (!reconnect_success && config_.enable_logging) {
                        std::lock_guard<std::mutex> lock(logging_mutex_);
                        std::cerr << "XSHM: Auto-reconnect failed after " << config_.max_retry_attempts << " attempts" << std::endl;
                    }
                    
                    // Reset reconnecting flag
                    reconnecting_.store(false);
                });
            }
        }
        was_connected = is_connected;
    }
    
private:
    AsyncXSHM() = default;
    
    
    void trigger_event(EventType type, const T* data) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_mutex_);
        for (auto& callback : callbacks_) {
            try {
                callback(type, data, nullptr);
            } catch (const std::exception&) {
                // Ignore errors in callbacks
            }
        }
    }
};

// ============================================================================
// CONVENIENT MACROS
// ============================================================================

// Macros for creating server/client
#define XSHM_CREATE_SERVER(type, name, size) \
    auto server = xshm::AsyncXSHM<type>::create_server(name, size)

#define XSHM_CREATE_SERVER_WITH_CONFIG(type, name, size, config) \
    auto server = xshm::AsyncXSHM<type>::create_server(name, size, config)

#define XSHM_CONNECT(type, name) \
    auto client = xshm::AsyncXSHM<type>::connect(name)

#define XSHM_CONNECT_WITH_CONFIG(type, name, config) \
    auto client = xshm::AsyncXSHM<type>::connect(name, config)

// ============================================================================
// USER-FRIENDLY MACROS - SIMPLE AND CLEAR
// ============================================================================

// 📤 SEND DATA (async, MOVE semantics) - SAFE VERSION
// send_sxc = "send server to client" = send from server to client
#define send_sxc(api, data) do { \
    auto future = (api)->send_to_client(std::move(data)); \
    if (!future.get()) { \
        std::cerr << "XSHM: Send SXC failed - buffer full" << std::endl; \
    } \
} while(0)

// send_cxs = "send client to server" = send from client to server  
#define send_cxs(api, data) do { \
    auto future = (api)->send_to_server(std::move(data)); \
    if (!future.get()) { \
        std::cerr << "XSHM: Send CXS failed - buffer full" << std::endl; \
    } \
} while(0)

// 📤 SEND DATA (async, COPY semantics) - PRESERVES ORIGINAL
#define send_sxc_copy(api, data) do { \
    auto future = (api)->send_to_client_copy(data); \
    if (!future.get()) { \
        std::cerr << "XSHM: Send SXC copy failed - buffer full" << std::endl; \
    } \
} while(0)

#define send_cxs_copy(api, data) do { \
    auto future = (api)->send_to_server_copy(data); \
    if (!future.get()) { \
        std::cerr << "XSHM: Send CXS copy failed - buffer full" << std::endl; \
    } \
} while(0)

    // ⏳ SEND DATA WITH RESULT WAITING (synchronous)
// send_sxc_wait = send from server to client and wait for result
#define send_sxc_wait(api, data) \
    (api)->send_to_client(std::move(data)).get()

// send_cxs_wait = send from client to server and wait for result
#define send_cxs_wait(api, data) \
    (api)->send_to_server(std::move(data)).get()

// 📥 RECEIVE DATA (automatically via events)
// recv_sxc = "receive server to client" = receive data from server (for client)
#define recv_sxc(api, callback) \
    (api)->on_data_received_sxc(callback)

// recv_cxs = "receive client to server" = receive data from client (for server)
#define recv_cxs(api, callback) \
    (api)->on_data_received_cxs(callback)

// Macros for other events
#define on_sent_sxc(api, callback) \
    (api)->on_data_sent_sxc(callback)

#define on_sent_cxs(api, callback) \
    (api)->on_data_sent_cxs(callback)

// 📋 COMPREHENSIVE CALLBACK MACROS
// on_data_received_sxc = callback for server-to-client data
#define on_data_received_sxc(api, callback) \
    (api)->on_data_received_sxc(callback)

// on_data_received_cxs = callback for client-to-server data
#define on_data_received_cxs(api, callback) \
    (api)->on_data_received_cxs(callback)

// on_data_sent_sxc = callback for server-to-client data sent
#define on_data_sent_sxc(api, callback) \
    (api)->on_data_sent_sxc(callback)

// on_data_sent_cxs = callback for client-to-server data sent
#define on_data_sent_cxs(api, callback) \
    (api)->on_data_sent_cxs(callback)

// on_connection_established = callback for connection established
#define on_connection_established(api, callback) \
    (api)->on_connection_established(callback)

// on_connection_failed = callback for connection failed
#define on_connection_failed(api, callback) \
    (api)->on_connection_failed(callback)

// 🔧 UTILITY MACROS
// is_connected = check if connection is active
#define XSHM_is_connected(api) \
    (api)->is_connected()

// get_statistics = get performance statistics
#define XSHM_get_statistics(api) \
    (api)->get_statistics()

// reset_statistics = reset performance counters
#define XSHM_reset_statistics(api) \
    (api)->reset_statistics()

// 🔗 CONNECTION EVENTS
// on_ready = "on ready" = when connection is ready for interaction
#define on_ready(api, callback) \
    (api)->on_connection_established(callback)

// on_connected = deprecated, use on_ready
#define on_connected(api, callback) \
    (api)->on_connection_established(callback)

// Macros for status checking
#define xshm_is_connected(api) \
    (api)->is_connected()

#define xshm_is_server(api) \
    (api)->is_server()

#define xshm_is_client(api) \
    (api)->is_client()

// Backward compatibility aliases (deprecated)
template<typename T>
using SmartSPSCQueue = RingBuffer<T>;

template<typename T>
using UltimateSPSCQueue = RingBuffer<T>;

template<typename T>
using UltimateDualQueue = DualRingBufferSystem<T>;

// ============================================================================
// CONVENIENT WRAPPER FOR ARBITRARY DATA
// ============================================================================

/**
 * @brief Simple wrapper for sending arbitrary binary data
 * @details Hides XSHM limitations and provides simple API for any data
 */
class XSHMessage {
private:
    std::unique_ptr<AsyncXSHM<uint8_t>> channel_;
    std::vector<uint8_t> receive_buffer_;
    std::function<void(const std::vector<uint8_t>&)> message_callback_;
    
public:
    /**
     * @brief Create server instance
     * @param name Unique name for the shared memory region
     * @param config Configuration (optional)
     */
    static std::unique_ptr<XSHMessage> create_server(std::string name, const XSHMConfig& config = XSHMConfig{}) {
        auto instance = std::unique_ptr<XSHMessage>(new XSHMessage());
        instance->channel_ = AsyncXSHM<uint8_t>::create_server(name, 1024, config);
        instance->setup_callbacks();
        return instance;
    }
    
    /**
     * @brief Connect to existing server
     * @param name Name of the shared memory region
     * @param config Configuration (optional)
     */
    static std::unique_ptr<XSHMessage> connect(std::string name, const XSHMConfig& config = XSHMConfig{}) {
        auto instance = std::unique_ptr<XSHMessage>(new XSHMessage());
        instance->channel_ = AsyncXSHM<uint8_t>::connect(name, config);
        instance->setup_callbacks();
        return instance;
    }
    
    /**
     * @brief Send arbitrary binary data
     * @param data Pointer to data
     * @param size Size of data in bytes
     * @return true if successful, false otherwise
     */
    bool send(const void* data, size_t size) {
        if (!channel_) return false;
        
        // Send size header (4 bytes)
        uint32_t size32 = static_cast<uint32_t>(size);
        if (!send_uint32(size32)) return false;
        
        // Send data
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            if (!channel_->send_to_client(ptr[i]).get()) {
                return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Send vector of bytes
     * @param data Vector containing data
     * @return true if successful, false otherwise
     */
    bool send(const std::vector<uint8_t>& data) {
        return send(data.data(), data.size());
    }
    
    /**
     * @brief Send string data
     * @param data String to send
     * @return true if successful, false otherwise
     */
    bool send(const std::string& data) {
        return send(data.data(), data.size());
    }
    
    /**
     * @brief Set callback for received messages
     * @param callback Function to call when message is received
     */
    void on_message(std::function<void(const std::vector<uint8_t>&)> callback) {
        message_callback_ = callback;
    }
    
    /**
     * @brief Check if connected
     * @return true if connected, false otherwise
     */
    bool is_connected() const {
        return channel_ && channel_->is_connected();
    }
    
    /**
     * @brief Get statistics
     * @return Statistics object
     */
    auto get_statistics() const {
        if (channel_) {
            return channel_->get_statistics();
        } else {
            // Return default statistics
            typename DualRingBufferSystem<uint8_t>::Statistics stats{};
            return stats;
        }
    }
    
private:
    XSHMessage() = default;
    
    void setup_callbacks() {
        if (!channel_) return;
        
        // Use direct method call instead of macro - disable macro temporarily
        #undef on_data_received_sxc
        channel_->on_data_received_sxc([this](const uint8_t* data) {
            if (data) {
                receive_buffer_.push_back(*data);
                
                // Check if we have complete message
                if (receive_buffer_.size() >= 4) {
                    uint32_t expected_size = *reinterpret_cast<uint32_t*>(receive_buffer_.data());
                    if (receive_buffer_.size() >= 4 + expected_size) {
                        // Complete message received!
                        std::vector<uint8_t> message(receive_buffer_.begin() + 4, 
                                                   receive_buffer_.begin() + 4 + expected_size);
                        if (message_callback_) {
                            message_callback_(message);
                        }
                        receive_buffer_.clear();
                    }
                }
            }
        });
        // Re-enable macro
        #define on_data_received_sxc(api, callback) \
            (api)->on_data_received_sxc(callback)
    }
    
    bool send_uint32(uint32_t value) {
        uint8_t bytes[4];
        bytes[0] = (value >> 0) & 0xFF;
        bytes[1] = (value >> 8) & 0xFF;
        bytes[2] = (value >> 16) & 0xFF;
        bytes[3] = (value >> 24) & 0xFF;
        
        for (int i = 0; i < 4; ++i) {
            if (!channel_->send_to_client(bytes[i]).get()) {
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// CONVENIENT MACROS FOR XSHMessage
// ============================================================================

// Create message server
#define XSHM_CREATE_MESSAGE_SERVER(name) \
    auto server = xshm::XSHMessage::create_server(name)

#define XSHM_CREATE_MESSAGE_SERVER_WITH_CONFIG(name, config) \
    auto server = xshm::XSHMessage::create_server(name, config)

// Connect to message server
#define XSHM_CONNECT_MESSAGE(name) \
    auto client = xshm::XSHMessage::connect(name)

#define XSHM_CONNECT_MESSAGE_WITH_CONFIG(name, config) \
    auto client = xshm::XSHMessage::connect(name, config)

// Send message
#define XSHM_SEND_MESSAGE(api, data) \
    (api)->send(data)

// Receive message
#define XSHM_ON_MESSAGE(api, callback) \
    (api)->on_message(callback)

// Restore Visual Studio warnings
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

} // namespace xshm