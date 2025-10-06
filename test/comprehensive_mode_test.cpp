#include "../xshm.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>

struct TestResult {
    std::string mode;
    uint64_t total_operations;
    uint64_t successful;
    uint64_t failed;
    uint64_t server_ops;
    uint64_t client_ops;
    uint64_t async_sent;
    uint64_t async_received;
    uint64_t sync_sent;
    uint64_t sync_received;
    uint64_t peak_ops_per_second;
    uint64_t average_ops_per_second;
    double success_rate;
    double failure_rate;
    int test_duration_seconds;
};

class ComprehensiveModeTest {
private:
    std::vector<TestResult> results_;
    std::mutex results_mutex_;
    std::ofstream report_file_;
    
public:
    ComprehensiveModeTest() {
        report_file_.open("comprehensive_mode_report.txt", std::ios::out | std::ios::trunc);
        logToFile("========================================");
        logToFile("XSHM COMPREHENSIVE MODE TEST");
        logToFile("========================================");
        logToFile("Testing both ASYNC and SYNC modes");
        logToFile("Each test runs for 30 seconds");
        logToFile("========================================");
    }
    
    ~ComprehensiveModeTest() {
        if (report_file_.is_open()) {
            generateComprehensiveReport();
            report_file_.close();
        }
    }
    
    void runAllTests() {
        // Тест 1: Асинхронный режим (без ожидания подтверждения)
        std::cout << "\n🚀 Starting ASYNC Mode Test..." << std::endl;
        runSingleTest(false, "ASYNC");
        
        // Небольшая пауза между тестами
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Тест 2: Синхронный режим (с ожиданием подтверждения)
        std::cout << "\n🔄 Starting SYNC Mode Test..." << std::endl;
        runSingleTest(true, "SYNC");
        
        // Генерируем сравнительный отчет
        generateComparisonReport();
    }
    
private:
    void runSingleTest(bool wait_for_confirmation, const std::string& mode_name) {
        // Создаем соединения
        xshm::XSHMConfig config;
        config.enable_logging = false;
        config.enable_auto_reconnect = true;
        config.event_loop_timeout_ms = 0;
        config.max_batch_size = 1;
        config.callback_thread_pool_size = 20;
        
        auto server = xshm::AsyncXSHM<uint32_t>::create_server("comprehensive_test_shm", 1024, config);
        auto client = xshm::AsyncXSHM<uint32_t>::connect("comprehensive_test_shm", config);
        
        // Статистика
        std::atomic<uint64_t> total_operations_{0};
        std::atomic<uint64_t> total_success_{0};
        std::atomic<uint64_t> total_failures_{0};
        std::atomic<uint64_t> server_ops_{0};
        std::atomic<uint64_t> client_ops_{0};
        std::atomic<uint64_t> async_sent_{0};
        std::atomic<uint64_t> async_received_{0};
        std::atomic<uint64_t> sync_sent_{0};
        std::atomic<uint64_t> sync_received_{0};
        std::atomic<uint64_t> max_ops_per_second_{0};
        
        std::atomic<bool> running_{false};
        auto test_start_time = std::chrono::steady_clock::now();
        
        // Настраиваем обработчики
        on_data_received_cxs(server, [&](const uint32_t* data) {
            if (data) {
                total_success_.fetch_add(1);
                async_received_.fetch_add(1);
            }
        });
        
        on_data_received_sxc(client, [&](const uint32_t* data) {
            if (data) {
                total_success_.fetch_add(1);
                async_received_.fetch_add(1);
            }
        });
        
        // Запускаем тест
        running_.store(true);
        
        std::thread server_thread([&]() {
            uint32_t id = 1;
            auto last_time = std::chrono::steady_clock::now();
            uint64_t ops_this_second = 0;
            
            while (running_.load()) {
                try {
                    uint32_t data = id++ * 100;
                    
                    if (wait_for_confirmation) {
                        // СИНХРОННАЯ отправка
                        auto future = server->send_to_client(data);
                        total_operations_.fetch_add(1);
                        server_ops_.fetch_add(1);
                        ops_this_second++;
                        
                        if (future.get()) {
                            total_success_.fetch_add(1);
                            sync_sent_.fetch_add(1);
                            sync_received_.fetch_add(1);
                        } else {
                            total_failures_.fetch_add(1);
                        }
                    } else {
                        // АСИНХРОННАЯ отправка
                        auto future = server->send_to_client(data);
                        total_operations_.fetch_add(1);
                        server_ops_.fetch_add(1);
                        ops_this_second++;
                        async_sent_.fetch_add(1);
                    }
                    
                    // Обновляем статистику производительности
                    if (ops_this_second % 1000 == 0) {
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                        
                        if (elapsed >= 1000) {
                            if (ops_this_second > max_ops_per_second_.load()) {
                                max_ops_per_second_.store(ops_this_second);
                            }
                            ops_this_second = 0;
                            last_time = now;
                        }
                    }
                    
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                    
                } catch (const std::exception& e) {
                    total_failures_.fetch_add(1);
                }
            }
        });
        
        std::thread client_thread([&]() {
            uint32_t id = 1;
            
            while (running_.load()) {
                try {
                    uint32_t data = id++ * 50;
                    
                    if (wait_for_confirmation) {
                        // СИНХРОННАЯ отправка
                        auto future = client->send_to_server(data);
                        total_operations_.fetch_add(1);
                        client_ops_.fetch_add(1);
                        
                        if (future.get()) {
                            total_success_.fetch_add(1);
                            sync_sent_.fetch_add(1);
                            sync_received_.fetch_add(1);
                        } else {
                            total_failures_.fetch_add(1);
                        }
                    } else {
                        // АСИНХРОННАЯ отправка
                        auto future = client->send_to_server(data);
                        total_operations_.fetch_add(1);
                        client_ops_.fetch_add(1);
                        async_sent_.fetch_add(1);
                    }
                    
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                    
                } catch (const std::exception& e) {
                    total_failures_.fetch_add(1);
                }
            }
        });
        
        // Ждем 30 секунд
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // Останавливаем
        running_.store(false);
        server_thread.join();
        client_thread.join();
        
        // Собираем результаты
        auto test_duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - test_start_time).count();
        
        TestResult result;
        result.mode = mode_name;
        result.total_operations = total_operations_.load();
        result.successful = total_success_.load();
        result.failed = total_failures_.load();
        result.server_ops = server_ops_.load();
        result.client_ops = client_ops_.load();
        result.async_sent = async_sent_.load();
        result.async_received = async_received_.load();
        result.sync_sent = sync_sent_.load();
        result.sync_received = sync_received_.load();
        result.peak_ops_per_second = max_ops_per_second_.load();
        result.average_ops_per_second = test_duration > 0 ? result.total_operations / test_duration : 0;
        result.success_rate = result.total_operations > 0 ? (double)result.successful / result.total_operations * 100.0 : 0.0;
        result.failure_rate = result.total_operations > 0 ? (double)result.failed / result.total_operations * 100.0 : 0.0;
        result.test_duration_seconds = test_duration;
        
        // Сохраняем результат
        {
            std::lock_guard<std::mutex> lock(results_mutex_);
            results_.push_back(result);
        }
        
        // Выводим промежуточные результаты
        std::cout << "✅ " << mode_name << " test completed!" << std::endl;
        std::cout << "   Operations: " << result.total_operations << std::endl;
        std::cout << "   Success Rate: " << std::fixed << std::setprecision(2) << result.success_rate << "%" << std::endl;
        std::cout << "   Average Ops/sec: " << result.average_ops_per_second << std::endl;
    }
    
    void generateComprehensiveReport() {
        logToFile("\n========================================");
        logToFile("COMPREHENSIVE MODE TEST - DETAILED RESULTS");
        logToFile("========================================");
        
        for (const auto& result : results_) {
            logToFile("\n--- " + result.mode + " MODE RESULTS ---");
            logToFile("Test Duration: " + std::to_string(result.test_duration_seconds) + " seconds");
            logToFile("Total Operations: " + std::to_string(result.total_operations));
            logToFile("Successful: " + std::to_string(result.successful) + " (" + 
                     std::to_string(result.success_rate) + "%)");
            logToFile("Failed: " + std::to_string(result.failed) + " (" + 
                     std::to_string(result.failure_rate) + "%)");
            logToFile("Server Operations: " + std::to_string(result.server_ops));
            logToFile("Client Operations: " + std::to_string(result.client_ops));
            logToFile("Async Sent: " + std::to_string(result.async_sent));
            logToFile("Async Received: " + std::to_string(result.async_received));
            logToFile("Sync Sent: " + std::to_string(result.sync_sent));
            logToFile("Sync Received: " + std::to_string(result.sync_received));
            logToFile("Peak Ops/Second: " + std::to_string(result.peak_ops_per_second));
            logToFile("Average Ops/Second: " + std::to_string(result.average_ops_per_second));
        }
    }
    
    void generateComparisonReport() {
        if (results_.size() < 2) return;
        
        const auto& async_result = results_[0];
        const auto& sync_result = results_[1];
        
        logToFile("\n========================================");
        logToFile("COMPARISON ANALYSIS");
        logToFile("========================================");
        
        // Производительность
        double ops_ratio = sync_result.average_ops_per_second > 0 ? 
            (double)async_result.average_ops_per_second / sync_result.average_ops_per_second : 0.0;
        
        logToFile("Performance Comparison:");
        logToFile("  ASYNC: " + std::to_string(async_result.average_ops_per_second) + " ops/sec");
        logToFile("  SYNC:  " + std::to_string(sync_result.average_ops_per_second) + " ops/sec");
        logToFile("  Ratio: " + std::to_string(ops_ratio) + "x (ASYNC vs SYNC)");
        
        // Надежность
        logToFile("\nReliability Comparison:");
        logToFile("  ASYNC Success Rate: " + std::to_string(async_result.success_rate) + "%");
        logToFile("  SYNC Success Rate:  " + std::to_string(sync_result.success_rate) + "%");
        
        // Эффективность
        logToFile("\nEfficiency Analysis:");
        logToFile("  ASYNC: " + std::to_string(async_result.async_sent) + " async sent, " + 
                 std::to_string(async_result.async_received) + " async received");
        logToFile("  SYNC:  " + std::to_string(sync_result.sync_sent) + " sync sent, " + 
                 std::to_string(sync_result.sync_received) + " sync received");
        
        // Рекомендации
        logToFile("\nRecommendations:");
        if (ops_ratio > 1.5) {
            logToFile("  ✅ ASYNC mode provides significantly higher throughput");
        } else if (ops_ratio < 0.7) {
            logToFile("  ✅ SYNC mode provides better performance");
        } else {
            logToFile("  ⚖️  Both modes provide similar performance");
        }
        
        if (async_result.success_rate > sync_result.success_rate + 10) {
            logToFile("  ✅ ASYNC mode provides better reliability");
        } else if (sync_result.success_rate > async_result.success_rate + 10) {
            logToFile("  ✅ SYNC mode provides better reliability");
        } else {
            logToFile("  ⚖️  Both modes provide similar reliability");
        }
        
        logToFile("========================================");
    }
    
    void logToFile(const std::string& message) {
        if (report_file_.is_open()) {
            report_file_ << "[" << getCurrentTimeString() << "] " << message << std::endl;
        }
        std::cout << message << std::endl;
    }
    
    std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::stringstream ss;
        ss << std::put_time(&tm, "%H:%M:%S");
        return ss.str();
    }
};

int main() {
    try {
        std::cout << "🔥 XSHM Comprehensive Mode Test" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "This test will run both ASYNC and SYNC modes" << std::endl;
        std::cout << "Each test runs for 30 seconds" << std::endl;
        std::cout << "Results will be saved to comprehensive_mode_report.txt" << std::endl;
        std::cout << "========================================" << std::endl;
        
        ComprehensiveModeTest test;
        test.runAllTests();
        
        std::cout << "\n✅ All tests completed!" << std::endl;
        std::cout << "📄 Full report: comprehensive_mode_report.txt" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
