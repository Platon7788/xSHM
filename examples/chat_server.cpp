#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    try {
        std::cout << "=== XSHM Chat Server Example ===" << std::endl;
        
        // Конфигурация для чата
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_activity_tracking = true;
        config.max_batch_size = 10;
        config.callback_thread_pool_size = 2;
        
        std::cout << "Creating chat server..." << std::endl;
        auto server = xshm::AsyncXSHM<uint32_t>::create_server("chat_app", 2048, config);
        
        // Обработчик подключения
        on_connection_established(server, []() {
            std::cout << "✅ Chat server is ready!" << std::endl;
        });
        
        // Обработчик сообщений от клиентов
        on_data_received_cxs(server, [](const uint32_t* message_id) {
            if (message_id) {
                std::cout << "💬 Message ID " << *message_id << " received from client" << std::endl;
            }
        });
        
        // Обработчик отправки сообщений
        on_data_sent_sxc(server, [](const uint32_t* message_id) {
            if (message_id) {
                std::cout << "📤 Message ID " << *message_id << " sent to clients" << std::endl;
            }
        });
        
        std::cout << "Chat server is running. Press Enter to send message, 'q' to quit..." << std::endl;
        
        uint32_t message_id = 1;
        std::string input;
        
        while (true) {
            std::cout << "Enter message ID (or 'q' to quit): ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                break;
            }
            
            if (!input.empty()) {
                try {
                    // Создаем сообщение
                    uint32_t msg_id = static_cast<uint32_t>(std::stoul(input));
                    
                    // Отправляем всем клиентам
                    send_sxc(server, msg_id);
                    
                    std::cout << "Message " << msg_id << " broadcasted to all clients!" << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Invalid input: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "Chat server shutting down..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Server Error: " << e.what() << std::endl;
        return 1;
    }
}