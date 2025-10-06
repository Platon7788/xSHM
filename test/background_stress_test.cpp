#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <sstream>

class BackgroundStressTest {
private:
    // Только один тип данных для максимальной скорости
    std::unique_ptr<xshm::AsyncXSHM<uint32_t>> uint32_server_;
    std::unique_ptr<xshm::AsyncXSHM<uint32_t>> uint32_client_;
    
    std::atomic<bool> running_{false};
    bool wait_for_confirmation_{false};  // Режим ожидания подтверждения
    
    // Статистика
    std::atomic<uint64_t> total_operations_{0};
    std::atomic<uint64_t> total_success_{0};
    std::atomic<uint64_t> total_failures_{0};
    std::atomic<uint64_t> server_ops_{0};
    std::atomic<uint64_t> client_ops_{0};
    std::atomic<uint64_t> max_ops_per_second_{0};
    std::atomic<uint64_t> current_ops_per_second_{0};
    
    // Дополнительная статистика для анализа
    std::atomic<uint64_t> async_sent_{0};
    std::atomic<uint64_t> async_received_{0};
    std::atomic<uint64_t> sync_sent_{0};
    std::atomic<uint64_t> sync_received_{0};
    
    // Временные метрики
    std::chrono::steady_clock::time_point test_start_time_;
    std::chrono::steady_clock::time_point last_stats_time_;
    
    // Файл для результатов (БЕЗ консоли!)
    std::ofstream results_file_;
    std::mutex file_mutex_;
    
public:
    BackgroundStressTest(bool wait_for_confirmation = false) : 
                           wait_for_confirmation_(wait_for_confirmation),
                           test_start_time_(std::chrono::steady_clock::now()),
                           last_stats_time_(std::chrono::steady_clock::now()) {
        
        // Открываем файл результатов
        results_file_.open("background_stress_results.txt", std::ios::out | std::ios::trunc);
        
        // Конфигурация для максимальной производительности
        xshm::XSHMConfig config;
        config.enable_logging = false;  // ОТКЛЮЧАЕМ ЛОГИРОВАНИЕ!
        config.enable_auto_reconnect = true;
        config.event_loop_timeout_ms = 0;  // МИНИМАЛЬНЫЙ таймаут
        config.max_batch_size = 1;  // Без батчинга для чистоты
        config.callback_thread_pool_size = 20;  // Большой пул потоков
        
        // Создаем соединения
        uint32_server_ = xshm::AsyncXSHM<uint32_t>::create_server("background_stress_shm", 1024, config);
        uint32_client_ = xshm::AsyncXSHM<uint32_t>::connect("background_stress_shm", config);
        
        // Настраиваем обработчики БЕЗ консольного вывода
        setupCallbacks();
        
        logToFile("Background Stress Test initialized");
    }
    
    ~BackgroundStressTest() {
        stop();
        if (results_file_.is_open()) {
            results_file_ << "========================================" << std::endl;
            results_file_ << "Test completed at: " << getCurrentTimeString() << std::endl;
            results_file_.close();
        }
    }
    
    void setupCallbacks() {
        // Сервер получает данные от клиента
        on_data_received_cxs(uint32_server_, [this](const uint32_t* data) {
            if (data) {
                // ТОЛЬКО счетчики, БЕЗ вывода в консоль!
                total_success_.fetch_add(1);
                async_received_.fetch_add(1);
            }
        });
        
        // Клиент получает данные от сервера  
        on_data_received_sxc(uint32_client_, [this](const uint32_t* data) {
            if (data) {
                // ТОЛЬКО счетчики, БЕЗ вывода в консоль!
                total_success_.fetch_add(1);
                async_received_.fetch_add(1);
            }
        });
    }
    
    void start() {
        running_.store(true);
        test_start_time_ = std::chrono::steady_clock::now();
        last_stats_time_ = test_start_time_;
        
        logToFile("Starting Background Stress Test...");
        
        // Запускаем потоки
        std::thread server_thread(&BackgroundStressTest::serverLoop, this);
        std::thread client_thread(&BackgroundStressTest::clientLoop, this);
        std::thread stats_thread(&BackgroundStressTest::statsLoop, this);
        
        // Ждем 30 секунд
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // Останавливаем
        running_.store(false);
        
        server_thread.join();
        client_thread.join();
        stats_thread.join();
        
        logToFile("Background Stress Test completed");
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
                
                if (wait_for_confirmation_) {
                    // СИНХРОННАЯ отправка С ожиданием результата
                    auto future = uint32_server_->send_to_client(data);
                    total_operations_.fetch_add(1);
                    server_ops_.fetch_add(1);
                    ops_this_second++;
                    
                    // ЖДЕМ подтверждения
                    if (future.get()) {
                        total_success_.fetch_add(1);
                        sync_sent_.fetch_add(1);
                        sync_received_.fetch_add(1);
                    } else {
                        total_failures_.fetch_add(1);
                    }
                } else {
                    // АСИНХРОННАЯ отправка БЕЗ ожидания результата
                    auto future = uint32_server_->send_to_client(data);
                    total_operations_.fetch_add(1);
                    server_ops_.fetch_add(1);
                    ops_this_second++;
                    
                    // НЕ ЖДЕМ future.get() - это блокирует!
                    async_sent_.fetch_add(1);
                    // total_success_ будет увеличиваться в обработчиках при получении
                }
                
                // Проверяем операции в секунду РЕЖЕ
                if (ops_this_second % 5000 == 0) { // Каждые 5000 операций
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                    
                    if (elapsed >= 1000) { // Каждую секунду
                        current_ops_per_second_.store(ops_this_second);
                        if (ops_this_second > max_ops_per_second_.load()) {
                            max_ops_per_second_.store(ops_this_second);
                        }
                        
                        logToFile("Server: " + std::to_string(ops_this_second) + " ops/sec (max: " + 
                                 std::to_string(max_ops_per_second_.load()) + ")");
                        
                        ops_this_second = 0;
                        last_time = now;
                    }
                }
                
                // МИНИМАЛЬНАЯ задержка
                std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                
            } catch (const std::exception& e) {
                total_failures_.fetch_add(1);
                logToFile("Server exception: " + std::string(e.what()));
            }
        }
    }
    
    void clientLoop() {
        uint32_t id = 1;
        auto last_time = std::chrono::steady_clock::now();
        uint64_t ops_this_second = 0;
        
        while (running_.load()) {
            try {
                uint32_t data = id++ * 50;
                
                if (wait_for_confirmation_) {
                    // СИНХРОННАЯ отправка С ожиданием результата
                    auto future = uint32_client_->send_to_server(data);
                    total_operations_.fetch_add(1);
                    client_ops_.fetch_add(1);
                    ops_this_second++;
                    
                    // ЖДЕМ подтверждения
                    if (future.get()) {
                        total_success_.fetch_add(1);
                        sync_sent_.fetch_add(1);
                        sync_received_.fetch_add(1);
                    } else {
                        total_failures_.fetch_add(1);
                    }
                } else {
                    // АСИНХРОННАЯ отправка БЕЗ ожидания результата
                    auto future = uint32_client_->send_to_server(data);
                    total_operations_.fetch_add(1);
                    client_ops_.fetch_add(1);
                    ops_this_second++;
                    
                    // НЕ ЖДЕМ future.get() - это блокирует!
                    async_sent_.fetch_add(1);
                    // total_success_ будет увеличиваться в обработчиках при получении
                }
                
                // Проверяем операции в секунду РЕЖЕ
                if (ops_this_second % 5000 == 0) { // Каждые 5000 операций
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                    
                    if (elapsed >= 1000) { // Каждую секунду
                        logToFile("Client: " + std::to_string(ops_this_second) + " ops/sec");
                        ops_this_second = 0;
                        last_time = now;
                    }
                }
                
                // МИНИМАЛЬНАЯ задержка
                std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                
            } catch (const std::exception& e) {
                total_failures_.fetch_add(1);
                logToFile("Client exception: " + std::string(e.what()));
            }
        }
    }
    
    void statsLoop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            uint64_t total_ops = total_operations_.load();
            uint64_t total_success = total_success_.load();
            uint64_t total_failures = total_failures_.load();
            uint64_t server_ops = server_ops_.load();
            uint64_t client_ops = client_ops_.load();
            uint64_t max_ops = max_ops_per_second_.load();
            uint64_t current_ops = current_ops_per_second_.load();
            
            // Вычисляем реальную производительность
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - test_start_time_).count();
            uint64_t real_ops_per_second = elapsed > 0 ? total_ops / elapsed : 0;
            
            uint64_t async_sent = async_sent_.load();
            uint64_t async_received = async_received_.load();
            uint64_t sync_sent = sync_sent_.load();
            uint64_t sync_received = sync_received_.load();
            
            logToFile("=== BACKGROUND STRESS STATISTICS ===");
            logToFile("Mode: " + std::string(wait_for_confirmation_ ? "SYNC (with confirmation)" : "ASYNC (no confirmation)"));
            logToFile("Total Operations: " + std::to_string(total_ops));
            logToFile("Successful: " + std::to_string(total_success) + " (" + 
                     std::to_string(total_ops > 0 ? (double)total_success / total_ops * 100.0 : 0.0) + "%)");
            logToFile("Failed: " + std::to_string(total_failures) + " (" + 
                     std::to_string(total_ops > 0 ? (double)total_failures / total_ops * 100.0 : 0.0) + "%)");
            logToFile("Server Operations: " + std::to_string(server_ops));
            logToFile("Client Operations: " + std::to_string(client_ops));
            logToFile("Async Sent: " + std::to_string(async_sent));
            logToFile("Async Received: " + std::to_string(async_received));
            logToFile("Sync Sent: " + std::to_string(sync_sent));
            logToFile("Sync Received: " + std::to_string(sync_received));
            logToFile("Peak Ops/Second: " + std::to_string(max_ops));
            logToFile("Current Ops/Second: " + std::to_string(current_ops));
            logToFile("Average Ops/Second: " + std::to_string(real_ops_per_second));
            logToFile("=====================================");
        }
    }
    
    void generateFinalReport() {
        auto test_duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - test_start_time_).count();
        
        uint64_t total_ops = total_operations_.load();
        uint64_t total_success = total_success_.load();
        uint64_t total_failures = total_failures_.load();
        uint64_t max_ops = max_ops_per_second_.load();
        uint64_t avg_ops = test_duration > 0 ? total_ops / test_duration : 0;
        
        uint64_t async_sent = async_sent_.load();
        uint64_t async_received = async_received_.load();
        uint64_t sync_sent = sync_sent_.load();
        uint64_t sync_received = sync_received_.load();
        
        logToFile("========================================");
        logToFile("BACKGROUND STRESS TEST - FINAL REPORT");
        logToFile("========================================");
        logToFile("Mode: " + std::string(wait_for_confirmation_ ? "SYNC (with confirmation)" : "ASYNC (no confirmation)"));
        logToFile("Test Duration: " + std::to_string(test_duration) + " seconds");
        logToFile("Total Operations: " + std::to_string(total_ops));
        logToFile("Successful: " + std::to_string(total_success) + " (" + 
                 std::to_string(total_ops > 0 ? (double)total_success / total_ops * 100.0 : 0.0) + "%)");
        logToFile("Failed: " + std::to_string(total_failures) + " (" + 
                 std::to_string(total_ops > 0 ? (double)total_failures / total_ops * 100.0 : 0.0) + "%)");
        logToFile("Async Sent: " + std::to_string(async_sent));
        logToFile("Async Received: " + std::to_string(async_received));
        logToFile("Sync Sent: " + std::to_string(sync_sent));
        logToFile("Sync Received: " + std::to_string(sync_received));
        logToFile("Peak Performance: " + std::to_string(max_ops) + " ops/sec");
        logToFile("Average Performance: " + std::to_string(avg_ops) + " ops/sec");
        logToFile("========================================");
        
        // Выводим результат в консоль только в конце
        std::cout << "\n🔥 BACKGROUND STRESS TEST COMPLETED!" << std::endl;
        std::cout << "📊 Peak Performance: " << max_ops << " ops/sec" << std::endl;
        std::cout << "📈 Average Performance: " << avg_ops << " ops/sec" << std::endl;
        std::cout << "📄 Full report: background_stress_results.txt" << std::endl;
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

int main(int argc, char* argv[]) {
    try {
        bool wait_for_confirmation = false;
        
        if (argc > 1 && std::string(argv[1]) == "--sync") {
            wait_for_confirmation = true;
        }
        
        std::cout << "🔥 Starting Background Stress Test..." << std::endl;
        std::cout << "⚡ NO CONSOLE OUTPUT during test for maximum speed!" << std::endl;
        std::cout << "📄 Results will be saved to background_stress_results.txt" << std::endl;
        std::cout << "⏱️  Test duration: 30 seconds" << std::endl;
        std::cout << "🔄 Mode: " << (wait_for_confirmation ? "SYNC (with confirmation)" : "ASYNC (no confirmation)") << std::endl;
        std::cout << "========================================" << std::endl;
        
        BackgroundStressTest test(wait_for_confirmation);
        test.start();
        
        std::cout << "\n✅ Background stress test completed!" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
