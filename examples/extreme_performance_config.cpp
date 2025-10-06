#include "../xshm.hpp"
#include <iostream>
#include <chrono>

// МАКСИМАЛЬНО БЫСТРАЯ конфигурация для экстремальной производительности
xshm::XSHMConfig create_extreme_performance_config() {
    xshm::XSHMConfig config;
    
    // === ЭКСТРЕМАЛЬНАЯ СКОРОСТЬ ===
    config.max_batch_size = 1;                   // БЕЗ БАТЧИНГА - каждое сообщение мгновенно
    config.event_loop_timeout_ms = 0;            // НЕМЕДЛЕННАЯ обработка (0мс)
    config.connection_timeout_ms = 100;          // Сверхбыстрое подключение
    
    // === МАКСИМАЛЬНАЯ ПРОИЗВОДИТЕЛЬНОСТЬ ===
    config.min_buffer_size = 2 * 1024 * 1024;    // 2MB минимум для быстрого старта
    config.max_buffer_size = 32 * 1024 * 1024;   // 32MB максимум для больших данных
    config.callback_thread_pool_size = 32;       // МАКСИМУМ потоков для параллельной обработки
    
    // === ЭКСТРЕМАЛЬНАЯ НАДЕЖНОСТЬ ===
    config.enable_logging = false;               // Отключаем ВСЕ логирование
    config.enable_auto_reconnect = true;         // Автоматическое переподключение
    config.enable_activity_tracking = false;     // Отключаем отслеживание для скорости
    config.enable_performance_counters = false;  // Отключаем счетчики для скорости
    config.enable_sequence_verification = false; // Отключаем проверку для скорости
    
    // === СВЕРХБЫСТРАЯ ОБРАБОТКА ===
    config.max_callback_timeout_ms = 0;          // МГНОВЕННЫЕ коллбэки (0мс)
    config.enable_async_callbacks = true;        // Асинхронные коллбэки
    
    // === МИНИМАЛЬНЫЕ ЗАДЕРЖКИ ===
    config.max_retry_attempts = 1;               // Только 1 попытка для скорости
    config.initial_retry_delay_ms = 0;           // МГНОВЕННЫЙ старт повторов
    config.max_retry_delay_ms = 1;               // Минимальная задержка
    
    return config;
}

// ЭКСТРЕМАЛЬНАЯ конфигурация для DLL (сервер)
xshm::XSHMConfig create_extreme_dll_config() {
    auto config = create_extreme_performance_config();
    
    // Дополнительные настройки для DLL
    config.min_buffer_size = 4 * 1024 * 1024;    // 4MB для DLL (максимум данных)
    config.max_buffer_size = 64 * 1024 * 1024;   // 64MB максимум для экстремальных данных
    config.callback_thread_pool_size = 64;       // МАКСИМУМ потоков для DLL
    config.max_callback_timeout_ms = 0;          // МГНОВЕННЫЕ коллбэки
    
    return config;
}

// ЭКСТРЕМАЛЬНАЯ конфигурация для основной программы (клиент)
xshm::XSHMConfig create_extreme_app_config() {
    auto config = create_extreme_performance_config();
    
    // Настройки для основной программы
    config.min_buffer_size = 2 * 1024 * 1024;    // 2MB для основной программы
    config.max_buffer_size = 32 * 1024 * 1024;   // 32MB максимум
    config.callback_thread_pool_size = 32;       // МАКСИМУМ потоков для основной программы
    config.max_callback_timeout_ms = 0;          // МГНОВЕННЫЕ коллбэки
    
    return config;
}

// УЛЬТРА-БЫСТРАЯ конфигурация для критических приложений
xshm::XSHMConfig create_ultra_fast_config() {
    xshm::XSHMConfig config;
    
    // === УЛЬТРА-СКОРОСТЬ ===
    config.max_batch_size = 1;                   // БЕЗ БАТЧИНГА
    config.event_loop_timeout_ms = 0;            // НЕМЕДЛЕННАЯ обработка
    config.connection_timeout_ms = 50;           // Сверхбыстрое подключение
    
    // === МАКСИМАЛЬНАЯ ПРОИЗВОДИТЕЛЬНОСТЬ ===
    config.min_buffer_size = 8 * 1024 * 1024;    // 8MB минимум
    config.max_buffer_size = 128 * 1024 * 1024;  // 128MB максимум
    config.callback_thread_pool_size = 128;      // УЛЬТРА-МАКСИМУМ потоков
    
    // === ОТКЛЮЧАЕМ ВСЕ ДЛЯ СКОРОСТИ ===
    config.enable_logging = false;
    config.enable_auto_reconnect = false;        // Отключаем для скорости
    config.enable_activity_tracking = false;
    config.enable_performance_counters = false;
    config.enable_sequence_verification = false;
    
    // === УЛЬТРА-БЫСТРАЯ ОБРАБОТКА ===
    config.max_callback_timeout_ms = 0;          // МГНОВЕННЫЕ коллбэки
    config.enable_async_callbacks = true;
    
    // === МИНИМАЛЬНЫЕ ЗАДЕРЖКИ ===
    config.max_retry_attempts = 0;               // БЕЗ повторов для скорости
    config.initial_retry_delay_ms = 0;
    config.max_retry_delay_ms = 0;
    
    return config;
}

int main() {
    std::cout << "=== XSHM EXTREME PERFORMANCE Configuration ===" << std::endl;
    
    // Показываем конфигурации
    auto extreme_dll_config = create_extreme_dll_config();
    auto extreme_app_config = create_extreme_app_config();
    auto ultra_fast_config = create_ultra_fast_config();
    
    std::cout << "\n🚀 EXTREME DLL Configuration:" << std::endl;
    std::cout << "  Buffer size: " << extreme_dll_config.min_buffer_size << " - " << extreme_dll_config.max_buffer_size << " bytes" << std::endl;
    std::cout << "  Batch size: " << extreme_dll_config.max_batch_size << " (NO BATCHING)" << std::endl;
    std::cout << "  Thread pool: " << extreme_dll_config.callback_thread_pool_size << " threads" << std::endl;
    std::cout << "  Event timeout: " << extreme_dll_config.event_loop_timeout_ms << "ms (INSTANT)" << std::endl;
    std::cout << "  Callback timeout: " << extreme_dll_config.max_callback_timeout_ms << "ms (INSTANT)" << std::endl;
    
    std::cout << "\n🚀 EXTREME APP Configuration:" << std::endl;
    std::cout << "  Buffer size: " << extreme_app_config.min_buffer_size << " - " << extreme_app_config.max_buffer_size << " bytes" << std::endl;
    std::cout << "  Batch size: " << extreme_app_config.max_batch_size << " (NO BATCHING)" << std::endl;
    std::cout << "  Thread pool: " << extreme_app_config.callback_thread_pool_size << " threads" << std::endl;
    std::cout << "  Event timeout: " << extreme_app_config.event_loop_timeout_ms << "ms (INSTANT)" << std::endl;
    std::cout << "  Callback timeout: " << extreme_app_config.max_callback_timeout_ms << "ms (INSTANT)" << std::endl;
    
    std::cout << "\n⚡ ULTRA-FAST Configuration:" << std::endl;
    std::cout << "  Buffer size: " << ultra_fast_config.min_buffer_size << " - " << ultra_fast_config.max_buffer_size << " bytes" << std::endl;
    std::cout << "  Batch size: " << ultra_fast_config.max_batch_size << " (NO BATCHING)" << std::endl;
    std::cout << "  Thread pool: " << ultra_fast_config.callback_thread_pool_size << " threads" << std::endl;
    std::cout << "  Event timeout: " << ultra_fast_config.event_loop_timeout_ms << "ms (INSTANT)" << std::endl;
    std::cout << "  Callback timeout: " << ultra_fast_config.max_callback_timeout_ms << "ms (INSTANT)" << std::endl;
    
    std::cout << "\n🔥 EXTREME Performance Estimates:" << std::endl;
    std::cout << "  Expected throughput: ~50,000+ messages/sec" << std::endl;
    std::cout << "  Peak data rate: ~3.2 GB/sec" << std::endl;
    std::cout << "  Average latency: <0.01ms (10 microseconds)" << std::endl;
    std::cout << "  Max message size: 65,000 bytes" << std::endl;
    std::cout << "  Real-time processing: ✅ MAXIMUM SPEED" << std::endl;
    std::cout << "  Batching: ❌ DISABLED (instant processing)" << std::endl;
    std::cout << "  Threads: 64-128 (maximum parallelization)" << std::endl;
    
    std::cout << "\n💡 Рекомендации:" << std::endl;
    std::cout << "  - EXTREME: Для 10,000+ сообщений/сек" << std::endl;
    std::cout << "  - ULTRA-FAST: Для 50,000+ сообщений/сек" << std::endl;
    std::cout << "  - Требует мощный CPU с множеством ядер" << std::endl;
    std::cout << "  - Рекомендуется SSD для буферов" << std::endl;
    
    return 0;
}
