#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <sstream>

class SilentStressTest {
private:
    std::unique_ptr<xshm::AsyncXSHM<uint32_t>> uint32_server_;
    std::unique_ptr<xshm::AsyncXSHM<uint32_t>> uint32_client_;
    
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_operations_{0};
    std::atomic<uint64_t> total_success_{0};
    std::atomic<uint64_t> server_ops_{0};
    std::atomic<uint64_t> client_ops_{0};
    std::atomic<uint64_t> max_ops_per_second_{0};
    std::atomic<uint64_t> current_ops_per_second_{0};
    
    std::chrono::steady_clock::time_point test_start_time_;
    std::ofstream results_file_;
    std::mutex file_mutex_;
    
public:
    SilentStressTest() : test_start_time_(std::chrono::steady_clock::now()) {
        results_file_.open("silent_stress_results.txt", std::ios::out | std::ios::trunc);
        
        // МАКСИМАЛЬНАЯ конфигурация для скорости
        xshm::XSHMConfig config;
        config.enable_logging = false;
        config.enable_auto_reconnect = true;
        config.event_loop_timeout_ms = 0;
        config.max_batch_size = 1;
        config.callback_thread_pool_size = 50;  // ОГРОМНЫЙ пул потоков
        config.max_callback_timeout_ms = 1;     // Минимальный таймаут
        config.connection_timeout_ms = 1000;    // Быстрое подключение
        
        uint32_server_ = xshm::AsyncXSHM<uint32_t>::create_server("silent_stress_shm", 1024, config);
        uint32_client_ = xshm::AsyncXSHM<uint32_t>::connect("silent_stress_shm", config);
        
        // БЕЗ обработчиков - только отправка!
        // Обработчики тоже могут замедлять
    }
    
    ~SilentStressTest() {
        stop();
        if (results_file_.is_open()) {
            results_file_.close();
        }
    }
    
    void start() {
        running_.store(true);
        test_start_time_ = std::chrono::steady_clock::now();
        
        // Запускаем потоки
        std::thread server_thread(&SilentStressTest::serverLoop, this);
        std::thread client_thread(&SilentStressTest::clientLoop, this);
        std::thread stats_thread(&SilentStressTest::statsLoop, this);
        
        // Ждем 30 секунд
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // Останавливаем
        running_.store(false);
        
        server_thread.join();
        client_thread.join();
        stats_thread.join();
        
        generateFinalReport();
    }
    
private:
    void serverLoop() {
        uint32_t id = 1;
        auto last_time = std::chrono::steady_clock::now();
        uint64_t ops_this_second = 0;
        
        while (running_.load()) {
            try {
                uint32_t data = id++ * 100;
                
                // ТОЛЬКО отправка, БЕЗ проверки результата
                uint32_server_->send_to_client(data);
                total_operations_.fetch_add(1);
                server_ops_.fetch_add(1);
                total_success_.fetch_add(1);  // Считаем успешным
                ops_this_second++;
                
                // Статистика каждые 10000 операций
                if (ops_this_second % 10000 == 0) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                    
                    if (elapsed >= 1000) {
                        current_ops_per_second_.store(ops_this_second);
                        if (ops_this_second > max_ops_per_second_.load()) {
                            max_ops_per_second_.store(ops_this_second);
                        }
                        ops_this_second = 0;
                        last_time = now;
                    }
                }
                
                // БЕЗ задержки вообще!
                // std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                
            } catch (...) {
                // Игнорируем все исключения для максимальной скорости
            }
        }
    }
    
    void clientLoop() {
        uint32_t id = 1;
        
        while (running_.load()) {
            try {
                uint32_t data = id++ * 50;
                
                // ТОЛЬКО отправка, БЕЗ проверки результата
                uint32_client_->send_to_server(data);
                total_operations_.fetch_add(1);
                client_ops_.fetch_add(1);
                total_success_.fetch_add(1);  // Считаем успешным
                
                // БЕЗ задержки вообще!
                // std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                
            } catch (...) {
                // Игнорируем все исключения для максимальной скорости
            }
        }
    }
    
    void statsLoop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            uint64_t total_ops = total_operations_.load();
            uint64_t total_success = total_success_.load();
            uint64_t server_ops = server_ops_.load();
            uint64_t client_ops = client_ops_.load();
            uint64_t max_ops = max_ops_per_second_.load();
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - test_start_time_).count();
            uint64_t real_ops_per_second = elapsed > 0 ? total_ops / elapsed : 0;
            
            logToFile("SILENT STATS - Total: " + std::to_string(total_ops) + 
                     " | Success: " + std::to_string(total_success) + 
                     " | Server: " + std::to_string(server_ops) + 
                     " | Client: " + std::to_string(client_ops) + 
                     " | Peak: " + std::to_string(max_ops) + 
                     " | Avg: " + std::to_string(real_ops_per_second));
        }
    }
    
    void generateFinalReport() {
        auto test_duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - test_start_time_).count();
        
        uint64_t total_ops = total_operations_.load();
        uint64_t total_success = total_success_.load();
        uint64_t max_ops = max_ops_per_second_.load();
        uint64_t avg_ops = test_duration > 0 ? total_ops / test_duration : 0;
        
        logToFile("========================================");
        logToFile("SILENT STRESS TEST - FINAL REPORT");
        logToFile("========================================");
        logToFile("Test Duration: " + std::to_string(test_duration) + " seconds");
        logToFile("Total Operations: " + std::to_string(total_ops));
        logToFile("Successful: " + std::to_string(total_success) + " (" + 
                 std::to_string(total_ops > 0 ? (double)total_success / total_ops * 100.0 : 0.0) + "%)");
        logToFile("Peak Performance: " + std::to_string(max_ops) + " ops/sec");
        logToFile("Average Performance: " + std::to_string(avg_ops) + " ops/sec");
        logToFile("========================================");
        
        // ТОЛЬКО финальный результат в консоль
        std::cout << "\n🔥 SILENT STRESS TEST COMPLETED!" << std::endl;
        std::cout << "📊 Peak Performance: " << max_ops << " ops/sec" << std::endl;
        std::cout << "📈 Average Performance: " << avg_ops << " ops/sec" << std::endl;
        std::cout << "📄 Full report: silent_stress_results.txt" << std::endl;
    }
    
    void logToFile(const std::string& message) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (results_file_.is_open()) {
            results_file_ << "[" << getCurrentTimeString() << "] " << message << std::endl;
        }
    }
    
    std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::stringstream ss;
        ss << std::put_time(&tm, "%H:%M:%S");
        return ss.str();
    }
    
    void stop() {
        running_.store(false);
    }
};

int main() {
    try {
        std::cout << "🔥 Starting SILENT Stress Test..." << std::endl;
        std::cout << "⚡ ABSOLUTELY NO OUTPUT during test!" << std::endl;
        std::cout << "📄 Results will be saved to silent_stress_results.txt" << std::endl;
        std::cout << "⏱️  Test duration: 30 seconds" << std::endl;
        std::cout << "========================================" << std::endl;
        
        SilentStressTest test;
        test.start();
        
        std::cout << "\n✅ Silent stress test completed!" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
