#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <random>

int main() {
    try {
        std::cout << "=== XSHM Safe Reading Client Example ===" << std::endl;
        
        // Конфигурация с включенной verification
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_sequence_verification = true;
        config.enable_activity_tracking = true;
        config.connection_timeout_ms = 5000;
        
        std::cout << "Connecting to safe reading server..." << std::endl;
        auto client = xshm::AsyncXSHM<uint32_t>::connect("safe_app", config);
        
        // Обработчик подключения
        on_connection_established(client, []() {
            std::cout << "✅ Connected to safe server!" << std::endl;
        });
        
        // Обработчик потери соединения
        on_connection_failed(client, []() {
            std::cout << "❌ Failed to connect to server" << std::endl;
        });
        
        // Обработчик данных с безопасным чтением
        on_data_received_sxc(client, [](const uint32_t* data) {
            if (data) {
                std::cout << "📨 Received safe data: " << *data << std::endl;
                
                // Проверка целостности
                if (*data % 2 == 0) {
                    std::cout << "   ✅ Data integrity verified!" << std::endl;
                } else {
                    std::cout << "   ❌ Data integrity check failed!" << std::endl;
                }
            }
        });
        
        std::cout << "Client is ready. Press Enter to start sending data, 'q' to quit..." << std::endl;
        
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "q" || input == "quit") {
            return 0;
        }
        
        // Генератор случайных данных
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> data_dist(1, 1000);
        std::uniform_int_distribution<> delay_dist(2000, 5000);
        
        bool sending = true;
        
        // Запускаем отправку данных в отдельном потоке
        std::thread sending_thread([&]() {
            while (sending) {
                // Генерируем безопасные данные (четные числа)
                uint32_t data = data_dist(gen) * 2; // Гарантируем четность
                
                // Отправляем серверу
                send_cxs(client, data);
                
                // Ждем случайное время (2-5 секунд)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
            }
        });
        
        // Основной цикл для пользовательского ввода
        while (true) {
            std::cout << "Press Enter to stop sending, 'q' to quit: ";
            std::getline(std::cin, input);
            
            if (input == "q" || input == "quit") {
                sending = false;
                break;
            }
            
            if (input.empty()) {
                sending = false;
                std::cout << "Sending stopped." << std::endl;
                break;
            }
        }
        
        // Ждем завершения потока отправки
        if (sending_thread.joinable()) {
            sending_thread.join();
        }
        
        std::cout << "Client disconnecting..." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Client Error: " << e.what() << std::endl;
        return 1;
    }
}