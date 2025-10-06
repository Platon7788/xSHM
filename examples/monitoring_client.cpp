#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>
#include <random>

int main() {
    try {
        std::cout << "=== XSHM Monitoring Client Example ===" << std::endl;
        
        // Продвинутая конфигурация клиента
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_auto_reconnect = true;
        config.enable_activity_tracking = true;
        config.enable_performance_counters = true;
        config.enable_sequence_verification = true;
        config.max_retry_attempts = 10;
        config.connection_timeout_ms = 5000;
        config.initial_retry_delay_ms = 100;
        
        std::cout << "Connecting to monitoring server..." << std::endl;
        auto client = xshm::AsyncXSHM<uint64_t>::connect("monitoring", config);
        
        // Обработчик подключения
        on_connection_established(client, []() {
            std::cout << "✅ Connected to monitoring server!" << std::endl;
        });
        
        // Обработчик потери соединения
        on_connection_failed(client, []() {
            std::cout << "❌ Failed to connect to server" << std::endl;
        });
        
        // Обработчик команд от сервера
        on_data_received_sxc(client, [](const uint64_t* cmd) {
            if (cmd) {
                std::cout << "📨 Server command: " << *cmd << std::endl;
            }
        });
        
        // Обработчик отправки метрик
        on_data_sent_cxs(client, [](const uint64_t* timestamp) {
            if (timestamp) {
                std::cout << "📤 Sent metric timestamp: " << *timestamp << std::endl;
            }
        });
        
        std::cout << "Client is ready. Press Enter to start monitoring, 'q' to quit..." << std::endl;
        
        std::string input;
        std::string hostname;
        
        std::cout << "Enter hostname: ";
        std::getline(std::cin, hostname);
        if (hostname.empty()) {
            hostname = "Client-" + std::to_string(std::time(nullptr) % 1000);
        }
        
        std::cout << "Press Enter to start monitoring, 'q' to quit..." << std::endl;
        std::getline(std::cin, input);
        
        if (input == "q" || input == "quit") {
            return 0;
        }
        
        // Генератор случайных данных для симуляции
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(1000, 5000);
        
        uint64_t metric_id = 1;
        bool monitoring = true;
        
        // Запускаем мониторинг в отдельном потоке
        std::thread monitoring_thread([&]() {
            while (monitoring) {
                // Генерируем timestamp метрики
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                
                // Отправляем серверу
                send_cxs(client, timestamp);
                
                // Ждем случайное время (1-5 секунд)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            }
        });
        
        // Основной цикл для пользовательского ввода
        while (true) {
            std::cout << "Press Enter to stop monitoring, 'q' to quit: ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                monitoring = false;
                break;
            }
            
            if (input.empty()) {
                monitoring = false;
                std::cout << "Monitoring stopped." << std::endl;
                break;
            }
        }
        
        // Ждем завершения потока мониторинга
        if (monitoring_thread.joinable()) {
            monitoring_thread.join();
        }
        
        std::cout << "Client disconnecting..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Client Error: " << e.what() << std::endl;
        return 1;
    }
}