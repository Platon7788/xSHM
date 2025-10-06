#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    try {
        std::cout << "=== XSHM Simple Server Example ===" << std::endl;
        
        // Конфигурация сервера
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_auto_reconnect = true;
        config.enable_activity_tracking = true;
        
        std::cout << "Creating server..." << std::endl;
        auto server = xshm::AsyncXSHM<uint32_t>::create_server("simple_app", 1024, config);
        
        // Обработчик подключения
        on_connection_established(server, []() {
            std::cout << "✅ Server is ready for connections!" << std::endl;
        });
        
        // Обработчик данных от клиентов
        on_data_received_cxs(server, [](const uint32_t* data) {
            if (data) {
                std::cout << "📨 Server received from client: " << *data << std::endl;
            }
        });
        
        // Обработчик отправки данных
        on_data_sent_sxc(server, [](const uint32_t* data) {
            if (data) {
                std::cout << "📤 Server sent to client: " << *data << std::endl;
            }
        });
        
        std::cout << "Server is running. Press Enter to send data, 'q' to quit..." << std::endl;
        
        uint32_t counter = 1;
        std::string input;
        
        while (true) {
            std::cout << "Press Enter to send data (or 'q' to quit): ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                break;
            }
            
            // Отправляем данные клиентам
            send_sxc(server, counter++);
            std::cout << "Data sent to clients: " << (counter - 1) << std::endl;
        }
        
        std::cout << "Server shutting down..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Server Error: " << e.what() << std::endl;
        return 1;
    }
}
