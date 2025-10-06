/**
 * @file xshmessage_example.cpp
 * @brief Example of using XSHMessage wrapper for arbitrary data
 */

#include "xshm.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== XSHMessage Example ===" << std::endl;
    
    try {
        // Create configuration
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_auto_reconnect = true;
        config.enable_statistics = true;
        config.event_loop_timeout_ms = 0;  // Real-time processing
        config.max_batch_size = 1;         // No batching
        
        // Create server with configuration
        auto server = xshm::XSHMessage::create_server("example_service", config);
        
        // Set up message handler
        server->on_message([](const std::vector<uint8_t>& data) {
            std::cout << "Server received " << data.size() << " bytes: ";
            for (uint8_t byte : data) {
                std::cout << std::hex << (int)byte << " ";
            }
            std::cout << std::dec << std::endl;
        });
        
        std::cout << "Server created, waiting for client..." << std::endl;
        
        // Wait a bit for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Create client with same configuration
        auto client = xshm::XSHMessage::connect("example_service", config);
        
        // Wait for connection
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (client->is_connected()) {
            std::cout << "Client connected!" << std::endl;
            
            // Send different types of data
            std::cout << "\nSending binary data..." << std::endl;
            std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04, 0x05};
            client->send(binary_data);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            std::cout << "Sending string data..." << std::endl;
            std::string text = "Hello XSHMessage!";
            client->send(text);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            std::cout << "Sending raw data..." << std::endl;
            const char* raw_data = "Raw binary data";
            client->send(raw_data, strlen(raw_data));
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Show statistics
            auto stats = client->get_statistics();
            std::cout << "\nClient statistics:" << std::endl;
            std::cout << "  Messages sent: " << stats.client_to_server_writes << std::endl;
            std::cout << "  Messages received: " << stats.client_to_server_reads << std::endl;
            
        } else {
            std::cout << "Failed to connect client!" << std::endl;
        }
        
        // Keep running for a bit
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nExample completed!" << std::endl;
    return 0;
}
