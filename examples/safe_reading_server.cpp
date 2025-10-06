#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    try {
        std::cout << "=== XSHM Safe Reading Server Example ===" << std::endl;
        
        // Конфигурация с включенной verification
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_sequence_verification = true;
        config.enable_activity_tracking = true;
        config.max_batch_size = 5;
        
        std::cout << "Creating server with safe reading..." << std::endl;
        auto server = xshm::AsyncXSHM<uint32_t>::create_server("safe_app", 1024, config);
        
        // Обработчик подключения
        on_connection_established(server, []() {
            std::cout << "✅ Safe server is ready!" << std::endl;
        });
        
        // Обработчик данных с безопасным чтением
        on_data_received_cxs(server, [](const uint32_t* data) {
            if (data) {
                std::cout << "📨 Received data ID: " << *data << std::endl;
                
                // Простая проверка целостности (четное число = валидное)
                if (*data % 2 == 0) {
                    std::cout << "   ✅ Data integrity verified (even number)!" << std::endl;
                } else {
                    std::cout << "   ❌ Data integrity check failed (odd number)!" << std::endl;
                }
            }
        });
        
        std::cout << "Server is running. Press Enter to send data, 'q' to quit..." << std::endl;
        
        uint32_t data_id = 1;
        std::string input;
        
        while (true) {
            std::cout << "Enter data value (or 'q' to quit): ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                break;
            }
            
            if (!input.empty()) {
                try {
                    // Создаем безопасные данные
                    uint32_t data = static_cast<uint32_t>(std::stoul(input));
                    
                    // Отправляем клиентам
                    send_sxc(server, data);
                    
                    std::cout << "Safe data " << data << " sent to clients!" << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Invalid input: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "Server shutting down..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Server Error: " << e.what() << std::endl;
        return 1;
    }
}