#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>

int main() {
    try {
        std::cout << "=== XSHM Monitoring Server Example ===" << std::endl;
        
        // Продвинутая конфигурация сервера
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_auto_reconnect = true;
        config.enable_activity_tracking = true;
        config.enable_performance_counters = true;
        config.enable_sequence_verification = true;
        config.max_batch_size = 20;
        config.callback_thread_pool_size = 4;
        config.max_callback_timeout_ms = 50;
        config.event_loop_timeout_ms = 100;
        
        std::cout << "Creating monitoring server..." << std::endl;
        auto server = xshm::AsyncXSHM<uint64_t>::create_server("monitoring", 2048, config);
        
        // Обработчик подключения
        on_connection_established(server, []() {
            std::cout << "✅ Monitoring server is ready!" << std::endl;
        });
        
        // Обработчик метрик от клиентов (используем uint64_t как timestamp)
        on_data_received_cxs(server, [](const uint64_t* timestamp) {
            if (timestamp) {
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                std::cout << "📊 Client metric received at: " 
                          << std::put_time(std::localtime(&time_t), "%H:%M:%S") << std::endl;
            }
        });
        
        // Обработчик отправки команд
        on_data_sent_sxc(server, [](const uint64_t* cmd) {
            if (cmd) {
                std::cout << "📤 Server sent command to client" << std::endl;
            }
        });
        
        std::cout << "Server is running. Press Enter to send a command, 'q' to quit..." << std::endl;
        
        uint64_t command_id = 1;
        std::string input;
        
        while (true) {
            std::cout << "Enter command (or 'q' to quit): ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                break;
            }
            
            if (!input.empty()) {
                // Создаем команду (используем uint64_t как ID команды)
                uint64_t cmd = command_id++;
                
                // Отправляем всем подключенным клиентам
                send_sxc(server, cmd);
                
                std::cout << "Command " << cmd << " sent to all clients!" << std::endl;
            }
            
            // Показываем статистику каждые 5 секунд
            static auto last_stats = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 5) {
                auto stats = XSHM_get_statistics(server);
                std::cout << "📈 Stats - SxC writes: " << stats.server_to_client_writes 
                          << " CxS reads: " << stats.client_to_server_reads 
                          << " Failed writes: " << stats.server_to_client_failed_writes << std::endl;
                last_stats = now;
            }
        }
        
        std::cout << "Server shutting down..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Server Error: " << e.what() << std::endl;
        return 1;
    }
}