#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <random>

int main() {
    try {
        std::cout << "=== XSHM Chat Client Example ===" << std::endl;
        
        // Конфигурация для чата
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_activity_tracking = true;
        config.connection_timeout_ms = 5000;
        
        std::cout << "Connecting to chat server..." << std::endl;
        auto client = xshm::AsyncXSHM<uint32_t>::connect("chat_app", config);
        
        // Обработчик подключения
        on_connection_established(client, []() {
            std::cout << "✅ Connected to chat server!" << std::endl;
        });
        
        // Обработчик потери соединения
        on_connection_failed(client, []() {
            std::cout << "❌ Failed to connect to chat server" << std::endl;
        });
        
        // Обработчик сообщений от сервера
        on_data_received_sxc(client, [](const uint32_t* message_id) {
            if (message_id) {
                std::cout << "💬 Received message ID: " << *message_id << std::endl;
            }
        });
        
        // Обработчик отправки сообщений
        on_data_sent_cxs(client, [](const uint32_t* message_id) {
            if (message_id) {
                std::cout << "📤 Sent message ID: " << *message_id << std::endl;
            }
        });
        
        std::cout << "Chat client is ready. Press Enter to start chatting, 'q' to quit..." << std::endl;
        
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "q" || input == "quit") {
            return 0;
        }
        
        // Генератор случайных сообщений
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> message_dist(1000, 9999);
        std::uniform_int_distribution<> delay_dist(3000, 8000);
        
        bool chatting = true;
        
        // Запускаем отправку сообщений в отдельном потоке
        std::thread chatting_thread([&]() {
            while (chatting) {
                // Генерируем случайное сообщение
                uint32_t message_id = message_dist(gen);
                
                // Отправляем серверу
                send_cxs(client, message_id);
                
                // Ждем случайное время (3-8 секунд)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            }
        });
        
        // Основной цикл для пользовательского ввода
        while (true) {
            std::cout << "Press Enter to stop chatting, 'q' to quit: ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                chatting = false;
                break;
            }
            
            if (input.empty()) {
                chatting = false;
                std::cout << "Chatting stopped." << std::endl;
                break;
            }
        }
        
        // Ждем завершения потока чата
        if (chatting_thread.joinable()) {
            chatting_thread.join();
        }
        
        std::cout << "Chat client disconnecting..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Client Error: " << e.what() << std::endl;
        return 1;
    }
}
