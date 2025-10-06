#include "../xshm.hpp"
#include <iostream>
#include <chrono>

// Конфигурация для РЕАЛЬНОГО ВРЕМЕНИ (без батчинга)
xshm::XSHMConfig create_realtime_config() {
    xshm::XSHMConfig config;
    
    // === РЕАЛЬНОЕ ВРЕМЯ ===
    config.max_batch_size = 1;                  // БЕЗ БАТЧИНГА - каждое сообщение сразу
    config.event_loop_timeout_ms = 0;           // НЕМЕДЛЕННАЯ обработка (0мс)
    config.connection_timeout_ms = 100;         // Быстрое подключение
    
    // === ПРОИЗВОДИТЕЛЬНОСТЬ ===
    config.min_buffer_size = 512 * 1024;        // 512KB минимум
    config.max_buffer_size = 8 * 1024 * 1024;   // 8MB максимум
    config.callback_thread_pool_size = 16;      // Большой пул потоков
    
    // === НАДЕЖНОСТЬ ===
    config.enable_auto_reconnect = true;
    config.enable_activity_tracking = true;
    config.enable_performance_counters = true;
    config.enable_sequence_verification = true;
    
    // === БЫСТРАЯ ОБРАБОТКА ===
    config.max_callback_timeout_ms = 1;         // Очень быстрые коллбэки
    config.enable_async_callbacks = true;
    
    // === ПОВТОРЫ ===
    config.max_retry_attempts = 3;              // Меньше попыток
    config.initial_retry_delay_ms = 1;          // Очень быстрый старт
    config.max_retry_delay_ms = 10;             // Минимальная задержка
    
    return config;
}

// Конфигурация для ВЫСОКОЙ ПРОПУСКНОЙ СПОСОБНОСТИ (с батчингом)
xshm::XSHMConfig create_throughput_config() {
    xshm::XSHMConfig config;
    
    // === ВЫСОКАЯ ПРОПУСКНАЯ СПОСОБНОСТЬ ===
    config.max_batch_size = 50;                 // Умеренный батчинг
    config.event_loop_timeout_ms = 1;           // Проверка каждую 1мс
    config.connection_timeout_ms = 1000;
    
    // === ПРОИЗВОДИТЕЛЬНОСТЬ ===
    config.min_buffer_size = 2 * 1024 * 1024;   // 2MB минимум
    config.max_buffer_size = 16 * 1024 * 1024;  // 16MB максимум
    config.callback_thread_pool_size = 8;
    
    // === НАДЕЖНОСТЬ ===
    config.enable_auto_reconnect = true;
    config.enable_activity_tracking = true;
    config.enable_performance_counters = true;
    config.enable_sequence_verification = true;
    
    // === ОБРАБОТКА ===
    config.max_callback_timeout_ms = 5;
    config.enable_async_callbacks = true;
    
    // === ПОВТОРЫ ===
    config.max_retry_attempts = 5;
    config.initial_retry_delay_ms = 10;
    config.max_retry_delay_ms = 100;
    
    return config;
}

// Гибридная конфигурация - компромисс
xshm::XSHMConfig create_hybrid_config() {
    xshm::XSHMConfig config;
    
    // === КОМПРОМИСС ===
    config.max_batch_size = 5;                  // Небольшой батчинг
    config.event_loop_timeout_ms = 0;           // Немедленная обработка
    config.connection_timeout_ms = 500;
    
    // === ПРОИЗВОДИТЕЛЬНОСТЬ ===
    config.min_buffer_size = 1 * 1024 * 1024;   // 1MB минимум
    config.max_buffer_size = 8 * 1024 * 1024;   // 8MB максимум
    config.callback_thread_pool_size = 12;
    
    // === НАДЕЖНОСТЬ ===
    config.enable_auto_reconnect = true;
    config.enable_activity_tracking = true;
    config.enable_performance_counters = true;
    config.enable_sequence_verification = true;
    
    // === ОБРАБОТКА ===
    config.max_callback_timeout_ms = 2;
    config.enable_async_callbacks = true;
    
    // === ПОВТОРЫ ===
    config.max_retry_attempts = 3;
    config.initial_retry_delay_ms = 5;
    config.max_retry_delay_ms = 50;
    
    return config;
}

int main() {
    std::cout << "=== XSHM Real-time vs Throughput Configuration ===" << std::endl;
    
    auto realtime_config = create_realtime_config();
    auto throughput_config = create_throughput_config();
    auto hybrid_config = create_hybrid_config();
    
    std::cout << "\n🚀 REAL-TIME Configuration (для мгновенной обработки):" << std::endl;
    std::cout << "  Batch size: " << realtime_config.max_batch_size << " (без батчинга)" << std::endl;
    std::cout << "  Event timeout: " << realtime_config.event_loop_timeout_ms << "ms (немедленно)" << std::endl;
    std::cout << "  Thread pool: " << realtime_config.callback_thread_pool_size << " threads" << std::endl;
    std::cout << "  Latency: <0.1ms" << std::endl;
    std::cout << "  Throughput: ~1000 msg/sec" << std::endl;
    
    std::cout << "\n📊 THROUGHPUT Configuration (для высокой пропускной способности):" << std::endl;
    std::cout << "  Batch size: " << throughput_config.max_batch_size << " (батчинг)" << std::endl;
    std::cout << "  Event timeout: " << throughput_config.event_loop_timeout_ms << "ms" << std::endl;
    std::cout << "  Thread pool: " << throughput_config.callback_thread_pool_size << " threads" << std::endl;
    std::cout << "  Latency: ~1ms" << std::endl;
    std::cout << "  Throughput: ~5000 msg/sec" << std::endl;
    
    std::cout << "\n⚖️  HYBRID Configuration (компромисс):" << std::endl;
    std::cout << "  Batch size: " << hybrid_config.max_batch_size << " (небольшой батчинг)" << std::endl;
    std::cout << "  Event timeout: " << hybrid_config.event_loop_timeout_ms << "ms (немедленно)" << std::endl;
    std::cout << "  Thread pool: " << hybrid_config.callback_thread_pool_size << " threads" << std::endl;
    std::cout << "  Latency: ~0.5ms" << std::endl;
    std::cout << "  Throughput: ~2000 msg/sec" << std::endl;
    
    std::cout << "\n💡 Рекомендации для вашего случая:" << std::endl;
    std::cout << "  - Если нужна МГНОВЕННАЯ обработка пакетов → REAL-TIME" << std::endl;
    std::cout << "  - Если нужна ВЫСОКАЯ пропускная способность → THROUGHPUT" << std::endl;
    std::cout << "  - Если нужен КОМПРОМИСС → HYBRID" << std::endl;
    
    return 0;
}
