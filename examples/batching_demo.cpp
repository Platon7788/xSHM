#include "../xshm.hpp"
#include <iostream>
#include <chrono>
#include <vector>

// Демонстрация батчинга
void demonstrate_batching() {
    std::cout << "=== XSHM Batching Demonstration ===" << std::endl;
    
    // Конфигурация с батчингом
    xshm::XSHMConfig config;
    config.max_batch_size = 5;  // Маленький батч для демонстрации
    config.event_loop_timeout_ms = 100;  // Проверка каждые 100мс
    
    auto server = xshm::AsyncXSHM<uint32_t>::create_server("batching_demo", 1024, config);
    
    // Счетчик отправленных сообщений
    int sent_count = 0;
    on_data_sent_sxc(server, [&](const uint32_t* data) {
        if (data) {
            sent_count++;
            std::cout << "📤 Sent message #" << *data << " (total sent: " << sent_count << ")" << std::endl;
        }
    });
    
    std::cout << "Sending 20 messages with batch_size=5..." << std::endl;
    
    // Отправляем 20 сообщений
    for (int i = 1; i <= 20; i++) {
        send_sxc(server, static_cast<uint32_t>(i));
        std::cout << "📝 Queued message #" << i << std::endl;
        
        // Небольшая задержка для демонстрации
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Ждем обработки всех сообщений
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "Total messages sent: " << sent_count << std::endl;
}

// Демонстрация без батчинга
void demonstrate_no_batching() {
    std::cout << "\n=== XSHM No Batching Demonstration ===" << std::endl;
    
    // Конфигурация без батчинга
    xshm::XSHMConfig config;
    config.max_batch_size = 1;  // Без батчинга
    config.event_loop_timeout_ms = 1;  // Быстрая обработка
    
    auto server = xshm::AsyncXSHM<uint32_t>::create_server("no_batching_demo", 1024, config);
    
    // Счетчик отправленных сообщений
    int sent_count = 0;
    on_data_sent_sxc(server, [&](const uint32_t* data) {
        if (data) {
            sent_count++;
            std::cout << "📤 Sent message #" << *data << " (total sent: " << sent_count << ")" << std::endl;
        }
    });
    
    std::cout << "Sending 20 messages with batch_size=1..." << std::endl;
    
    // Отправляем 20 сообщений
    for (int i = 1; i <= 20; i++) {
        send_sxc(server, static_cast<uint32_t>(i));
        std::cout << "📝 Queued message #" << i << std::endl;
        
        // Небольшая задержка для демонстрации
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Ждем обработки всех сообщений
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "Total messages sent: " << sent_count << std::endl;
}

int main() {
    try {
        // Демонстрация с батчингом
        demonstrate_batching();
        
        // Демонстрация без батчинга
        demonstrate_no_batching();
        
        std::cout << "\n=== Batching Benefits ===" << std::endl;
        std::cout << "✅ With batching: Fewer system calls, better throughput" << std::endl;
        std::cout << "❌ Without batching: More system calls, higher overhead" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
