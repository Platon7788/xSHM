# xSHM - High-Performance Shared Memory IPC Library

[![License: Free](https://img.shields.io/badge/License-Free-green.svg)](https://github.com/Platon/xSHM)

**Author:** Platon  
**Language:** C (Windows-native)  
**Platforms:** Windows 10/11 (x86/x64, cross-architecture compatible)  
**Status:** Production-ready, actively maintained  

xSHM is a lightweight, high-performance shared memory IPC library for Windows. It uses zero-copy overwriting ring buffers for bidirectional communication between processes, with automatic connection detection and event-driven notifications. Optimized for low-latency, high-throughput scenarios like real-time data exchange.

## Описание / Description

### English
A cross-architecture (x86/x64) shared memory IPC library with lock-free overwriting ring buffers, automatic client detection via active readers, and asynchronous events (including CONNECT/DISCONNECT). Supports zero-copy messaging up to 64KB per message, with bidirectional channels and easy macros for rapid prototyping. Ideal for inter-process communication in games, simulations, or embedded systems on Windows.

### Русский
Библиотека межпроцессного взаимодействия (IPC) через разделяемую память для Windows с кросс-архитектурной поддержкой (x86/x64). Использует безблокирующие overwriting-кольцевые буферы, автоматическое обнаружение подключения клиента через active_readers и асинхронные события (включая CONNECT/DISCONNECT). Поддерживает zero-copy сообщения до 64 КБ, двунаправленные каналы и удобные макросы для быстрой разработки. Идеально для межпроцессного обмена данными в играх, симуляциях или встраиваемых системах на Windows.

## Features

- **Zero-Copy Overwriting Buffers**: Always succeeds on write (overwrites old data if full), no blocking.
- **Automatic Connection Management**: Detects client connect/disconnect via ring buffer active readers—no manual handshakes.
- **Event-Driven Architecture**: Async callbacks for data availability, connect/disconnect, errors; background listener with configurable timeout.
- **Bidirectional Communication**: Separate TX/RX rings for server<->client.
- **Cross-Architecture**: Seamless x86/x64 mixed-mode (same machine).
- **High Performance**: Sub-ms latency, GB/s throughput; lock-free with atomics and events.
- **Easy API**: Macros for structs, default configs; blocking/non-blocking reads.
- **Thread-Safe**: Safe multi-threaded access per process.

## Architecture

### Core Components

1. **Ring Buffer** (`shm_ringbuffer.c/h`)
   - Overwriting circular buffer (power-of-2 size).
   - Atomic positions (Interlocked ops); supports multiple readers.
   - Header with active_readers for connection detection.
   - Events for data/space (data always signaled on write).

2. **Event System** (`shm_events.c/h`)
   - Windows Events for signaling (DATA_AVAILABLE, SPACE_AVAILABLE, DISCONNECT, ERROR, CONNECT).
   - Callback registration with auto-starting listener thread (100ms timeout).
   - Periodic status checks on server for connect/disconnect.

3. **IPC Layer** (`shm_ipc.c/h`)
   - Server creates rings/events; client opens with retries.
   - Auto-detection: Server monitors tx_ring->active_readers.
   - Send ignores if not connected; receive always attempts.

## Building

### Prerequisites
- Windows 10/11
- Visual Studio 2019/2022 (C/C++ tools)
- CMake 3.15+ (optional, for future builds)

### Build Scripts
Run in project root:

#### All Architectures
```batch
build_all.bat
```

#### x64 Only
```batch
build_x64.bat
```

#### x86 Only
```batch
build_x86.bat
```

Outputs: `build_x64/Release/` and `build_x86/Release/` (DLLs, EXEs, libs).

## Quick Start

### Server Setup
```c
#include "shm_ipc.h"

shm_ring_config_t config = SHM_DEFAULT_CONFIG();  // 4MB rings, blocking reads
shm_server_t* server = shm_server_create("test_channel", &config);
if (!server) { /* error */ }

// Auto-start listener on register
shm_error_t err = shm_server_register_callback(server, my_handler, NULL);
```

### Client Connection
```c
shm_client_t* client = shm_client_connect("test_channel");
if (!client) { /* error */ }

// Register with auto-start
shm_client_register_callback(client, my_handler, NULL);
```

### Sending/Receiving
```c
// Send (always succeeds if size <= 64KB and connected for server)
char msg[] = "Hello!";
shm_server_send(server, msg, sizeof(msg));  // Server to client
shm_client_send(client, msg, sizeof(msg));  // Client to server

// Receive (blocks if empty and blocking=true)
uint8_t buf[1024]; uint32_t sz = sizeof(buf);
shm_error_t res = shm_server_receive(server, buf, &sz);  // From client
if (res == SHM_SUCCESS) { /* process buf[0..sz] */ }
```

### Event Handler
```c
void my_handler(void* ud, shm_event_type_t type, const void* data, uint32_t sz) {
    switch (type) {
        case SHM_EVENT_CONNECT: printf("Client connected!\n"); break;
        case SHM_EVENT_DISCONNECT: printf("Client disconnected!\n"); break;
        case SHM_EVENT_DATA_AVAILABLE: /* poll receive */ break;
        case SHM_EVENT_ERROR: printf("Error!\n"); break;
    }
}
```

### Structs (Macros)
```c
typedef struct { uint32_t id; char text[256]; } my_struct_t;
my_struct_t s = { .id = 42 };
SHM_SEND_STRUCT(server, &s);  // Server/client generic

my_struct_t r;
uint32_t rsz = sizeof(r);
SHM_RECV_STRUCT(server, &r);  // Updates rsz
```

### Cleanup
```c
shm_server_destroy(server);
shm_client_disconnect(client);
```

## API Reference

### Config
```c
typedef struct {
    uint32_t size;        // Power-of-2, e.g., 4*1024*1024 (4MB)
    uint32_t max_readers; // Max active readers
    bool blocking;        // Block on empty read?
    uint32_t timeout_ms;  // Read timeout
} shm_ring_config_t;
#define SHM_DEFAULT_CONFIG() ((shm_ring_config_t){.size=4*1024*1024, .max_readers=4, .blocking=true, .timeout_ms=5000})
```

### Server API
```c
shm_server_t* shm_server_create(const char* name, const shm_ring_config_t* config);
void shm_server_destroy(shm_server_t* server);
shm_error_t shm_server_send(shm_server_t* server, const void* data, uint32_t size);  // Ignores if !connected
shm_error_t shm_server_receive(shm_server_t* server, void* data, uint32_t* size);   // Blocks if empty
shm_error_t shm_server_register_callback(shm_server_t* server, shm_event_callback_t cb, void* ud);
```

### Client API
```c
shm_client_t* shm_client_connect(const char* name);  // Retries ~500ms
void shm_client_disconnect(shm_client_t* client);
shm_error_t shm_client_send(shm_client_t* client, const void* data, uint32_t size);
shm_error_t shm_client_receive(shm_client_t* client, void* data, uint32_t* size);
shm_error_t shm_client_register_callback(shm_client_t* client, shm_event_callback_t cb, void* ud);
```

### Events
```c
typedef enum {
    SHM_EVENT_DATA_AVAILABLE = 0,
    SHM_EVENT_SPACE_AVAILABLE = 1,  // Less relevant for overwriting
    SHM_EVENT_DISCONNECT = 2,
    SHM_EVENT_ERROR = 3,
    SHM_EVENT_CONNECT = 4
} shm_event_type_t;
typedef void (*shm_event_callback_t)(void* ud, shm_event_type_t type, const void* data, uint32_t sz);
```

### Errors
```c
typedef enum {
    SHM_SUCCESS = 0,
    SHM_ERROR_INVALID_PARAM = -1,
    SHM_ERROR_MEMORY = -2,
    SHM_ERROR_TIMEOUT = -3,
    SHM_ERROR_EMPTY = -4,      // Non-blocking read failed
    SHM_ERROR_EXISTS = -5,
    SHM_ERROR_NOT_FOUND = -6,
    SHM_ERROR_ACCESS = -7
} shm_error_t;
```

### Macros (Convenience)
- `SHM_DEFAULT_CONFIG()`: 4MB default.
- `SHM_CREATE_SERVER(name)`: Server with default.
- `SHM_CONNECT_CLIENT(name)`: Client connect.
- `SHM_SERVER_SEND(server, data, size)` / `SHM_CLIENT_SEND(...)`: Send.
- `SHM_SERVER_RECEIVE(...)` / `SHM_CLIENT_RECEIVE(...)`: Receive.
- `SHM_SERVER_ON_EVENT(server, cb, ud)` / `SHM_CLIENT_ON_EVENT(...)`: Register + start listener.
- `SHM_SETUP_SERVER(name, cb, ud)` / `SHM_SETUP_CLIENT(...)`: Quick setup.
- `SHM_SEND_STRUCT(endpoint, ptr)` / `SHM_RECV_STRUCT(endpoint, ptr)`: Generic for structs.

## Examples

Build and run from `build_x64/Release/` (or x86).

1. **Simple Echo Server/Client**:
   ```batch
   example_server.exe  // Starts server, waits for client
   example_client.exe  // Connects, sends/receives
   ```

2. **Bi-Dir Stress Test** (100k msgs each way):
   ```batch
   test_server.exe     // Server waits for connect
   test_client.exe     // Client connects, runs stress
   ```
   Outputs reports: `server_report.txt`, `test_log.txt`. Measures latency, throughput, loss.

3. **Mixed Arch Test**:
   Run server x64: `example_mixed_arch.exe server`
   Run client x86: `example_mixed_arch.exe` (in x86 build).

## Performance

- **Latency**: <1ms round-trip (measured in stress test: avg 0.5ms median).
- **Throughput**: Up to 100+ MB/s (stress: ~50 MB/s peak on i7).
- **Overhead**: Minimal—atomic ops + events; no locks.
- **Scalability**: Handles 10k+ msgs/s; overwriting prevents stalls.
- Tested: 100k msgs bi-dir with <0.1% loss on 4MB buffers.

## Best Practices

- **Buffer Size**: Start with 4MB; scale for msg volume.
- **Msg Limits**: <=64KB/msg; use overwriting for bursts.
- **Events**: Use CONNECT/DISCONNECT for lifecycle; poll DATA_AVAILABLE sparingly.
- **Errors**: Check `SHM_ERROR_EMPTY` for non-blocking; log others.
- **Cleanup**: Always destroy/disconnect to decrement active_readers.
- **Debug**: Enable printf in handlers; monitor buffer usage via `shm_ring_available()`.

## Technical Details

- **Memory**: FileMapping + MapViewOfFile; header + buffer layout.
- **Sync**: Interlocked for positions/readers; MemoryBarrier; Events for notifies.
- **Connect Detect**: Server polls tx_ring->active_readers (periodic + on send/recv).
- **Overwriting**: Write always succeeds; drops oldest if full (CAS for read_pos advance).
- **x86/x64**: Fixed types, no packing issues; tested mixed.

## License

Free for any use. No warranties. (MIT-like, but simplified.)

## Troubleshooting

- **Connect Fail**: Run server first; check name match; admin if perms issue.
- **No Events**: Ensure callback registered before connect; check timeout.
- **High Loss**: Increase buffer; reduce msg rate; verify overwriting logic.
- **Mixed Arch**: Rebuild both; same struct defs.

## Contributing

This is an open-source reference impl. Contributions welcome!

- **Issues/PRs**: Report bugs, suggest features (e.g., Linux port, multi-client).
- **Feedback**: Test on your hardware/use-case; share perf numbers.
- **Help Wanted**: Docs, examples, optimizations—DM or open issue.

Прошу обратную связь и помощь в развитии! (Feedback and collaboration appreciated—let's make it better together.) Reach out: [GitHub Issues](https://github.com/Platon/xSHM/issues).
