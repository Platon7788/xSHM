#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    try {
        std::cout << "=== XSHM Simple Client Example ===" << std::endl;
        
        // Конфигурация клиента
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_auto_reconnect = true;
        config.enable_activity_tracking = true;
        config.max_retry_attempts = 5;
        config.connection_timeout_ms = 3000;
        
        std::cout << "Connecting to server..." << std::endl;
        auto client = xshm::AsyncXSHM<uint32_t>::connect("simple_app", config);
        
        // Обработчик подключения
        on_connection_established(client, []() {
            std::cout << "✅ Connected to server!" << std::endl;
        });
        
        // Обработчик потери соединения
        on_connection_failed(client, []() {
            std::cout << "❌ Failed to connect to server" << std::endl;
        });
        
        // Обработчик данных от сервера
        on_data_received_sxc(client, [](const uint32_t* data) {
            if (data) {
                std::cout << "📨 Client received from server: " << *data << std::endl;
            }
        });
        
        // Обработчик отправки данных
        on_data_sent_cxs(client, [](const uint32_t* data) {
            if (data) {
                std::cout << "📤 Client sent to server: " << *data << std::endl;
            }
        });
        
        std::cout << "Client is ready. Press Enter to send data, 'q' to quit..." << std::endl;
        
        uint32_t counter = 1000;
        std::string input;
        
        while (true) {
            std::cout << "Press Enter to send data (or 'q' to quit): ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                break;
            }
            
            // Отправляем данные серверу
            send_cxs(client, counter++);
            std::cout << "Data sent to server: " << (counter - 1) << std::endl;
        }
        
        std::cout << "Client disconnecting..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Client Error: " << e.what() << std::endl;
        return 1;
    }
}
