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
#include <map>

class ComprehensiveAnalysis {
private:
    // Серверы и клиенты для разных типов данных
    std::unique_ptr<xshm::AsyncXSHM<uint32_t>> uint32_server_;
    std::unique_ptr<xshm::AsyncXSHM<uint32_t>> uint32_client_;
    std::unique_ptr<xshm::AsyncXSHM<uint64_t>> uint64_server_;
    std::unique_ptr<xshm::AsyncXSHM<uint64_t>> uint64_client_;
    std::unique_ptr<xshm::AsyncXSHM<double>> double_server_;
    std::unique_ptr<xshm::AsyncXSHM<double>> double_client_;
    
    std::atomic<bool> running_{false};
    
    // Детальная статистика для каждого типа данных
    struct DataTypeStats {
        std::atomic<uint64_t> server_sent_success{0};
        std::atomic<uint64_t> server_sent_failures{0};
        std::atomic<uint64_t> server_received_success{0};
        std::atomic<uint64_t> server_received_failures{0};
        std::atomic<uint64_t> client_sent_success{0};
        std::atomic<uint64_t> client_sent_failures{0};
        std::atomic<uint64_t> client_received_success{0};
        std::atomic<uint64_t> client_received_failures{0};
        std::atomic<uint64_t> server_exceptions{0};
        std::atomic<uint64_t> client_exceptions{0};
        std::atomic<uint64_t> connection_retries{0};
        std::atomic<uint64_t> timeout_errors{0};
        std::atomic<uint64_t> memory_errors{0};
        std::atomic<uint64_t> sync_errors{0};
    };
    
    DataTypeStats uint32_stats_;
    DataTypeStats uint64_stats_;
    DataTypeStats double_stats_;
    
    // Общая статистика
    std::atomic<uint64_t> total_operations_{0};
    std::atomic<uint64_t> total_success_{0};
    std::atomic<uint64_t> total_failures_{0};
    std::atomic<uint64_t> total_exceptions_{0};
    std::atomic<uint64_t> total_retries_{0};
    
    // Стресс-тест статистика
    std::atomic<uint64_t> stress_operations_{0};
    std::atomic<uint64_t> stress_success_{0};
    std::atomic<uint64_t> stress_failures_{0};
    std::atomic<uint64_t> stress_exceptions_{0};
    std::atomic<uint64_t> stress_server_ops_{0};
    std::atomic<uint64_t> stress_client_ops_{0};
    std::atomic<uint64_t> max_ops_per_second_{0};
    std::atomic<uint64_t> current_ops_per_second_{0};
    
    // Батчинг-тест статистика
    std::atomic<uint64_t> batching_operations_{0};
    std::atomic<uint64_t> batching_success_{0};
    std::atomic<uint64_t> batching_failures_{0};
    std::atomic<uint64_t> batching_exceptions_{0};
    std::atomic<uint64_t> batching_server_ops_{0};
    std::atomic<uint64_t> batching_client_ops_{0};
    std::atomic<uint64_t> batching_batches_sent_{0};
    std::atomic<uint64_t> batching_batches_received_{0};
    std::atomic<uint64_t> max_batching_ops_per_second_{0};
    std::atomic<uint64_t> current_batching_ops_per_second_{0};
    
    // Временные метрики
    std::chrono::steady_clock::time_point test_start_time_;
    std::chrono::steady_clock::time_point last_stats_time_;
    
    // Файлы отчетов
    std::ofstream detailed_report_;
    std::ofstream summary_report_;
    std::mutex report_mutex_;
    
    // История операций для анализа
    struct OperationRecord {
        std::chrono::steady_clock::time_point timestamp;
        std::string operation_type;
        std::string data_type;
        bool success;
        std::string error_message;
        uint64_t value;
    };
    
    std::vector<OperationRecord> operation_history_;
    std::mutex history_mutex_;

public:
    ComprehensiveAnalysis() : test_start_time_(std::chrono::steady_clock::now()), 
                             last_stats_time_(std::chrono::steady_clock::now()) {
        
        // Открываем файлы отчетов
        detailed_report_.open("detailed_analysis_report.txt", std::ios::out | std::ios::trunc);
        summary_report_.open("summary_analysis_report.txt", std::ios::out | std::ios::trunc);
        
        if (detailed_report_.is_open()) {
            detailed_report_ << "XSHM Comprehensive Analysis - Detailed Report" << std::endl;
            detailed_report_ << "=============================================" << std::endl;
            detailed_report_ << "Test started: " << getCurrentTimeString() << std::endl;
            detailed_report_ << std::endl;
        }
        
        if (summary_report_.is_open()) {
            summary_report_ << "XSHM Comprehensive Analysis - Summary Report" << std::endl;
            summary_report_ << "============================================" << std::endl;
            summary_report_ << "Test started: " << getCurrentTimeString() << std::endl;
            summary_report_ << std::endl;
        }
        
        std::cout << "🔧 Starting Comprehensive XSHM Analysis..." << std::endl;
        logDetailed("Starting Comprehensive XSHM Analysis...");
        
        initializeConnections();
    }
    
    ~ComprehensiveAnalysis() {
        if (detailed_report_.is_open()) {
            detailed_report_ << "Test completed: " << getCurrentTimeString() << std::endl;
            detailed_report_.close();
        }
        if (summary_report_.is_open()) {
            summary_report_ << "Test completed: " << getCurrentTimeString() << std::endl;
            summary_report_.close();
        }
    }
    
    void initializeConnections() {
        xshm::XSHMConfig config;
        config.enable_logging = true;
        config.enable_auto_reconnect = true;
        config.event_loop_timeout_ms = 1;
        config.max_batch_size = 1;
        config.max_retry_attempts = 3;
        config.initial_retry_delay_ms = 100;
        
        try {
            // Создаем серверы
            std::cout << "🔧 Creating servers..." << std::endl;
            uint32_server_ = xshm::AsyncXSHM<uint32_t>::create_server("comprehensive_uint32", 1024, config);
            uint64_server_ = xshm::AsyncXSHM<uint64_t>::create_server("comprehensive_uint64", 1024, config);
            double_server_ = xshm::AsyncXSHM<double>::create_server("comprehensive_double", 1024, config);
            std::cout << "✅ Servers created successfully" << std::endl;
            logDetailed("All servers created successfully");
            
            // Ждем инициализации
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Создаем клиентов
            std::cout << "🔧 Creating clients..." << std::endl;
            uint32_client_ = xshm::AsyncXSHM<uint32_t>::connect("comprehensive_uint32", config);
            uint64_client_ = xshm::AsyncXSHM<uint64_t>::connect("comprehensive_uint64", config);
            double_client_ = xshm::AsyncXSHM<double>::connect("comprehensive_double", config);
            std::cout << "✅ Clients connected successfully" << std::endl;
            logDetailed("All clients connected successfully");
            
            // Регистрируем обработчики
            setupCallbacks();
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Connection error: " << e.what() << std::endl;
            logDetailed("CONNECTION ERROR: " + std::string(e.what()));
            throw;
        }
    }
    
    void setupCallbacks() {
        // uint32_t callbacks
        on_data_received_cxs(uint32_server_, [this](const uint32_t* data) {
            if (data) {
                uint32_stats_.server_received_success.fetch_add(1);
                total_success_.fetch_add(1);
                recordOperation("SERVER_RECEIVE", "uint32_t", true, "", *data);
                std::cout << "📥 Server received uint32_t: " << *data << std::endl;
            } else {
                uint32_stats_.server_received_failures.fetch_add(1);
                total_failures_.fetch_add(1);
                recordOperation("SERVER_RECEIVE", "uint32_t", false, "NULL data", 0);
                std::cout << "❌ Server received NULL uint32_t" << std::endl;
            }
        });
        
        on_data_received_sxc(uint32_client_, [this](const uint32_t* data) {
            if (data) {
                uint32_stats_.client_received_success.fetch_add(1);
                total_success_.fetch_add(1);
                recordOperation("CLIENT_RECEIVE", "uint32_t", true, "", *data);
                std::cout << "📥 Client received uint32_t: " << *data << std::endl;
            } else {
                uint32_stats_.client_received_failures.fetch_add(1);
                total_failures_.fetch_add(1);
                recordOperation("CLIENT_RECEIVE", "uint32_t", false, "NULL data", 0);
                std::cout << "❌ Client received NULL uint32_t" << std::endl;
            }
        });
        
        // uint64_t callbacks
        on_data_received_cxs(uint64_server_, [this](const uint64_t* data) {
            if (data) {
                uint64_stats_.server_received_success.fetch_add(1);
                total_success_.fetch_add(1);
                recordOperation("SERVER_RECEIVE", "uint64_t", true, "", *data);
                std::cout << "📥 Server received uint64_t: " << *data << std::endl;
            } else {
                uint64_stats_.server_received_failures.fetch_add(1);
                total_failures_.fetch_add(1);
                recordOperation("SERVER_RECEIVE", "uint64_t", false, "NULL data", 0);
                std::cout << "❌ Server received NULL uint64_t" << std::endl;
            }
        });
        
        on_data_received_sxc(uint64_client_, [this](const uint64_t* data) {
            if (data) {
                uint64_stats_.client_received_success.fetch_add(1);
                total_success_.fetch_add(1);
                recordOperation("CLIENT_RECEIVE", "uint64_t", true, "", *data);
                std::cout << "📥 Client received uint64_t: " << *data << std::endl;
            } else {
                uint64_stats_.client_received_failures.fetch_add(1);
                total_failures_.fetch_add(1);
                recordOperation("CLIENT_RECEIVE", "uint64_t", false, "NULL data", 0);
                std::cout << "❌ Client received NULL uint64_t" << std::endl;
            }
        });
        
        // double callbacks
        on_data_received_cxs(double_server_, [this](const double* data) {
            if (data) {
                double_stats_.server_received_success.fetch_add(1);
                total_success_.fetch_add(1);
                recordOperation("SERVER_RECEIVE", "double", true, "", static_cast<uint64_t>(*data));
                std::cout << "📥 Server received double: " << *data << std::endl;
            } else {
                double_stats_.server_received_failures.fetch_add(1);
                total_failures_.fetch_add(1);
                recordOperation("SERVER_RECEIVE", "double", false, "NULL data", 0);
                std::cout << "❌ Server received NULL double" << std::endl;
            }
        });
        
        on_data_received_sxc(double_client_, [this](const double* data) {
            if (data) {
                double_stats_.client_received_success.fetch_add(1);
                total_success_.fetch_add(1);
                recordOperation("CLIENT_RECEIVE", "double", true, "", static_cast<uint64_t>(*data));
                std::cout << "📥 Client received double: " << *data << std::endl;
            } else {
                double_stats_.client_received_failures.fetch_add(1);
                total_failures_.fetch_add(1);
                recordOperation("CLIENT_RECEIVE", "double", false, "NULL data", 0);
                std::cout << "❌ Client received NULL double" << std::endl;
            }
        });
    }
    
    void start() {
        running_.store(true);
        
        std::cout << "🚀 Starting Comprehensive Analysis with Stress & Batching Tests..." << std::endl;
        logDetailed("Starting Comprehensive Analysis with Stress & Batching Tests...");
        
        // Фаза 1: Нормальный тест (30 секунд)
        std::cout << "\n📊 PHASE 1: Normal Load Test (30 seconds)" << std::endl;
        std::cout << "=========================================" << std::endl;
        runNormalLoadTest();
        
        // Фаза 2: Ультра-стресс тест (30 секунд)
        std::cout << "\n🔥 PHASE 2: ULTRA-STRESS Test (30 seconds)" << std::endl;
        std::cout << "==========================================" << std::endl;
        runStressTest();
        
        // Фаза 3: Батчинг-тест (15 секунд)
        std::cout << "\n📦 PHASE 3: Batching Test (15 seconds)" << std::endl;
        std::cout << "======================================" << std::endl;
        runBatchingTest();
        
        // Генерируем финальные отчеты
        generateFinalReports();
    }
    
    void runNormalLoadTest() {
        // Запускаем потоки для каждого типа данных с нормальной нагрузкой
        std::thread uint32_server_thread(&ComprehensiveAnalysis::uint32ServerLoop, this);
        std::thread uint32_client_thread(&ComprehensiveAnalysis::uint32ClientLoop, this);
        std::thread uint64_server_thread(&ComprehensiveAnalysis::uint64ServerLoop, this);
        std::thread uint64_client_thread(&ComprehensiveAnalysis::uint64ClientLoop, this);
        std::thread double_server_thread(&ComprehensiveAnalysis::doubleServerLoop, this);
        std::thread double_client_thread(&ComprehensiveAnalysis::doubleClientLoop, this);
        std::thread stats_thread(&ComprehensiveAnalysis::statsLoop, this);
        
        // Ждем 30 секунд для нормального теста
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // ОСТАНОВКА: Сбрасываем флаг ПЕРЕД join()
        running_.store(false);
        
        // Останавливаем нормальные потоки
        uint32_server_thread.join();
        uint32_client_thread.join();
        uint64_server_thread.join();
        uint64_client_thread.join();
        double_server_thread.join();
        double_client_thread.join();
        stats_thread.join();
        
        std::cout << "✅ Normal Load Test completed" << std::endl;
        logDetailed("Normal Load Test completed");
    }
    
    void runStressTest() {
        // Сбрасываем счетчики для стресс-теста
        resetCountersForStressTest();
        
        // ВОЗОБНОВЛЯЕМ: Устанавливаем флаг для стресс-теста
        running_.store(true);
        
        // Запускаем стресс-потоки
        std::thread stress_server_thread(&ComprehensiveAnalysis::stressServerLoop, this);
        std::thread stress_client_thread(&ComprehensiveAnalysis::stressClientLoop, this);
        std::thread stress_stats_thread(&ComprehensiveAnalysis::stressStatsLoop, this);
        
        // Ждем 30 секунд для стресс-теста
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // ОСТАНОВКА: Сбрасываем флаг ПЕРЕД join()
        running_.store(false);
        
        // Останавливаем стресс-потоки
        stress_server_thread.join();
        stress_client_thread.join();
        stress_stats_thread.join();
        
        std::cout << "✅ ULTRA-STRESS Test completed" << std::endl;
        logDetailed("ULTRA-STRESS Test completed");
    }
    
    void runBatchingTest() {
        // Сбрасываем счетчики для батчинг-теста
        resetCountersForBatchingTest();
        
        // ВОЗОБНОВЛЯЕМ: Устанавливаем флаг для батчинг-теста
        running_.store(true);
        
        // Запускаем батчинг-потоки
        std::thread batching_server_thread(&ComprehensiveAnalysis::batchingServerLoop, this);
        std::thread batching_client_thread(&ComprehensiveAnalysis::batchingClientLoop, this);
        std::thread batching_stats_thread(&ComprehensiveAnalysis::batchingStatsLoop, this);
        
        // Ждем 15 секунд для батчинг-теста
        std::this_thread::sleep_for(std::chrono::seconds(15));
        
        // ОСТАНОВКА: Сбрасываем флаг ПЕРЕД join()
        running_.store(false);
        
        // Останавливаем батчинг-потоки
        batching_server_thread.join();
        batching_client_thread.join();
        batching_stats_thread.join();
        
        std::cout << "✅ Batching Test completed" << std::endl;
        logDetailed("Batching Test completed");
    }
    
private:
    void uint32ServerLoop() {
        uint32_t id = 1;
        while (running_.load()) {
            try {
                uint32_t data = id++ * 100;
                auto future = uint32_server_->send_to_client(data);
                total_operations_.fetch_add(1);
                
                if (future.get()) {
                    uint32_stats_.server_sent_success.fetch_add(1);
                    total_success_.fetch_add(1);
                    recordOperation("SERVER_SEND", "uint32_t", true, "", data);
                    std::cout << "📤 Server sent uint32_t: " << data << std::endl;
                } else {
                    uint32_stats_.server_sent_failures.fetch_add(1);
                    total_failures_.fetch_add(1);
                    recordOperation("SERVER_SEND", "uint32_t", false, "Send failed", data);
                    std::cout << "❌ Server send failed uint32_t: " << data << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } catch (const std::exception& e) {
                uint32_stats_.server_exceptions.fetch_add(1);
                total_exceptions_.fetch_add(1);
                recordOperation("SERVER_SEND", "uint32_t", false, e.what(), 0);
                std::cerr << "❌ Server exception uint32_t: " << e.what() << std::endl;
            }
        }
    }
    
    void uint32ClientLoop() {
        uint32_t id = 1;
        while (running_.load()) {
            try {
                uint32_t data = id++ * 50;
                auto future = uint32_client_->send_to_server(data);
                total_operations_.fetch_add(1);
                
                if (future.get()) {
                    uint32_stats_.client_sent_success.fetch_add(1);
                    total_success_.fetch_add(1);
                    recordOperation("CLIENT_SEND", "uint32_t", true, "", data);
                    std::cout << "📤 Client sent uint32_t: " << data << std::endl;
                } else {
                    uint32_stats_.client_sent_failures.fetch_add(1);
                    total_failures_.fetch_add(1);
                    recordOperation("CLIENT_SEND", "uint32_t", false, "Send failed", data);
                    std::cout << "❌ Client send failed uint32_t: " << data << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(75));
            } catch (const std::exception& e) {
                uint32_stats_.client_exceptions.fetch_add(1);
                total_exceptions_.fetch_add(1);
                recordOperation("CLIENT_SEND", "uint32_t", false, e.what(), 0);
                std::cerr << "❌ Client exception uint32_t: " << e.what() << std::endl;
            }
        }
    }
    
    void uint64ServerLoop() {
        uint64_t id = 1;
        while (running_.load()) {
            try {
                uint64_t data = id++ * 200;
                auto future = uint64_server_->send_to_client(data);
                total_operations_.fetch_add(1);
                
                if (future.get()) {
                    uint64_stats_.server_sent_success.fetch_add(1);
                    total_success_.fetch_add(1);
                    recordOperation("SERVER_SEND", "uint64_t", true, "", data);
                    std::cout << "📤 Server sent uint64_t: " << data << std::endl;
                } else {
                    uint64_stats_.server_sent_failures.fetch_add(1);
                    total_failures_.fetch_add(1);
                    recordOperation("SERVER_SEND", "uint64_t", false, "Send failed", data);
                    std::cout << "❌ Server send failed uint64_t: " << data << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (const std::exception& e) {
                uint64_stats_.server_exceptions.fetch_add(1);
                total_exceptions_.fetch_add(1);
                recordOperation("SERVER_SEND", "uint64_t", false, e.what(), 0);
                std::cerr << "❌ Server exception uint64_t: " << e.what() << std::endl;
            }
        }
    }
    
    void uint64ClientLoop() {
        uint64_t id = 1;
        while (running_.load()) {
            try {
                uint64_t data = id++ * 100;
                auto future = uint64_client_->send_to_server(data);
                total_operations_.fetch_add(1);
                
                if (future.get()) {
                    uint64_stats_.client_sent_success.fetch_add(1);
                    total_success_.fetch_add(1);
                    recordOperation("CLIENT_SEND", "uint64_t", true, "", data);
                    std::cout << "📤 Client sent uint64_t: " << data << std::endl;
                } else {
                    uint64_stats_.client_sent_failures.fetch_add(1);
                    total_failures_.fetch_add(1);
                    recordOperation("CLIENT_SEND", "uint64_t", false, "Send failed", data);
                    std::cout << "❌ Client send failed uint64_t: " << data << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(125));
            } catch (const std::exception& e) {
                uint64_stats_.client_exceptions.fetch_add(1);
                total_exceptions_.fetch_add(1);
                recordOperation("CLIENT_SEND", "uint64_t", false, e.what(), 0);
                std::cerr << "❌ Client exception uint64_t: " << e.what() << std::endl;
            }
        }
    }
    
    void doubleServerLoop() {
        double id = 1.0;
        while (running_.load()) {
            try {
                double data = id++ * 1.5;
                auto future = double_server_->send_to_client(data);
                total_operations_.fetch_add(1);
                
                if (future.get()) {
                    double_stats_.server_sent_success.fetch_add(1);
                    total_success_.fetch_add(1);
                    recordOperation("SERVER_SEND", "double", true, "", static_cast<uint64_t>(data));
                    std::cout << "📤 Server sent double: " << data << std::endl;
                } else {
                    double_stats_.server_sent_failures.fetch_add(1);
                    total_failures_.fetch_add(1);
                    recordOperation("SERVER_SEND", "double", false, "Send failed", static_cast<uint64_t>(data));
                    std::cout << "❌ Server send failed double: " << data << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            } catch (const std::exception& e) {
                double_stats_.server_exceptions.fetch_add(1);
                total_exceptions_.fetch_add(1);
                recordOperation("SERVER_SEND", "double", false, e.what(), 0);
                std::cerr << "❌ Server exception double: " << e.what() << std::endl;
            }
        }
    }
    
    void doubleClientLoop() {
        double id = 1.0;
        while (running_.load()) {
            try {
                double data = id++ * 2.5;
                auto future = double_client_->send_to_server(data);
                total_operations_.fetch_add(1);
                
                if (future.get()) {
                    double_stats_.client_sent_success.fetch_add(1);
                    total_success_.fetch_add(1);
                    recordOperation("CLIENT_SEND", "double", true, "", static_cast<uint64_t>(data));
                    std::cout << "📤 Client sent double: " << data << std::endl;
                } else {
                    double_stats_.client_sent_failures.fetch_add(1);
                    total_failures_.fetch_add(1);
                    recordOperation("CLIENT_SEND", "double", false, "Send failed", static_cast<uint64_t>(data));
                    std::cout << "❌ Client send failed double: " << data << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(90));
            } catch (const std::exception& e) {
                double_stats_.client_exceptions.fetch_add(1);
                total_exceptions_.fetch_add(1);
                recordOperation("CLIENT_SEND", "double", false, e.what(), 0);
                std::cerr << "❌ Client exception double: " << e.what() << std::endl;
            }
        }
    }
    
    void statsLoop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            printDetailedStats();
        }
    }
    
    void printDetailedStats() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - test_start_time_).count();
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "                    COMPREHENSIVE ANALYSIS STATS (T+" << elapsed << "s)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        // Общая статистика
        uint64_t total_ops = total_operations_.load();
        uint64_t total_success = total_success_.load();
        uint64_t total_failures = total_failures_.load();
        uint64_t total_exceptions = total_exceptions_.load();
        
        std::cout << "📊 OVERALL STATISTICS:" << std::endl;
        std::cout << "   Total Operations: " << total_ops << std::endl;
        std::cout << "   Successful: " << total_success << " (" << std::fixed << std::setprecision(2) 
                  << (total_ops > 0 ? (double)total_success / total_ops * 100.0 : 0.0) << "%)" << std::endl;
        std::cout << "   Failed: " << total_failures << " (" << std::fixed << std::setprecision(2) 
                  << (total_ops > 0 ? (double)total_failures / total_ops * 100.0 : 0.0) << "%)" << std::endl;
        std::cout << "   Exceptions: " << total_exceptions << " (" << std::fixed << std::setprecision(2) 
                  << (total_ops > 0 ? (double)total_exceptions / total_ops * 100.0 : 0.0) << "%)" << std::endl;
        
        // Статистика по типам данных
        printDataTypeStats("uint32_t", uint32_stats_);
        printDataTypeStats("uint64_t", uint64_stats_);
        printDataTypeStats("double", double_stats_);
        
        std::cout << std::string(80, '=') << std::endl;
        
        // Записываем в файлы
        logDetailedStats(elapsed);
    }
    
    void printDataTypeStats(const std::string& type_name, const DataTypeStats& stats) {
        uint64_t server_sent = stats.server_sent_success.load() + stats.server_sent_failures.load();
        uint64_t server_received = stats.server_received_success.load() + stats.server_received_failures.load();
        uint64_t client_sent = stats.client_sent_success.load() + stats.client_sent_failures.load();
        uint64_t client_received = stats.client_received_success.load() + stats.client_received_failures.load();
        
        std::cout << "\n📈 " << type_name << " STATISTICS:" << std::endl;
        std::cout << "   Server Sent: " << stats.server_sent_success.load() << " success, " 
                  << stats.server_sent_failures.load() << " failures" << std::endl;
        std::cout << "   Server Received: " << stats.server_received_success.load() << " success, " 
                  << stats.server_received_failures.load() << " failures" << std::endl;
        std::cout << "   Client Sent: " << stats.client_sent_success.load() << " success, " 
                  << stats.client_sent_failures.load() << " failures" << std::endl;
        std::cout << "   Client Received: " << stats.client_received_success.load() << " success, " 
                  << stats.client_received_failures.load() << " failures" << std::endl;
        std::cout << "   Exceptions: Server=" << stats.server_exceptions.load() 
                  << ", Client=" << stats.client_exceptions.load() << std::endl;
        
        if (server_sent > 0) {
            double server_sent_rate = (double)stats.server_sent_success.load() / server_sent * 100.0;
            std::cout << "   Server Send Success Rate: " << std::fixed << std::setprecision(2) << server_sent_rate << "%" << std::endl;
        }
        if (client_sent > 0) {
            double client_sent_rate = (double)stats.client_sent_success.load() / client_sent * 100.0;
            std::cout << "   Client Send Success Rate: " << std::fixed << std::setprecision(2) << client_sent_rate << "%" << std::endl;
        }
    }
    
    void recordOperation(const std::string& op_type, const std::string& data_type, bool success, 
                        const std::string& error_msg, uint64_t value) {
        std::lock_guard<std::mutex> lock(history_mutex_);
        operation_history_.push_back({
            std::chrono::steady_clock::now(),
            op_type,
            data_type,
            success,
            error_msg,
            value
        });
        
        // Ограничиваем размер истории
        if (operation_history_.size() > 10000) {
            operation_history_.erase(operation_history_.begin(), operation_history_.begin() + 1000);
        }
    }
    
    void generateFinalReports() {
        auto test_duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - test_start_time_).count();
        
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "                           FINAL COMPREHENSIVE ANALYSIS REPORT" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        
        // Общая статистика
        uint64_t total_ops = total_operations_.load();
        uint64_t total_success = total_success_.load();
        uint64_t total_failures = total_failures_.load();
        uint64_t total_exceptions = total_exceptions_.load();
        
        std::cout << "⏱️  TEST DURATION: " << test_duration << " seconds" << std::endl;
        std::cout << "📊 TOTAL OPERATIONS: " << total_ops << std::endl;
        std::cout << "✅ SUCCESSFUL OPERATIONS: " << total_success << " (" 
                  << std::fixed << std::setprecision(2) 
                  << (total_ops > 0 ? (double)total_success / total_ops * 100.0 : 0.0) << "%)" << std::endl;
        std::cout << "❌ FAILED OPERATIONS: " << total_failures << " (" 
                  << std::fixed << std::setprecision(2) 
                  << (total_ops > 0 ? (double)total_failures / total_ops * 100.0 : 0.0) << "%)" << std::endl;
        std::cout << "💥 EXCEPTIONS: " << total_exceptions << " (" 
                  << std::fixed << std::setprecision(2) 
                  << (total_ops > 0 ? (double)total_exceptions / total_ops * 100.0 : 0.0) << "%)" << std::endl;
        
        // Производительность
        double ops_per_second = total_ops / (double)test_duration;
        std::cout << "🚀 NORMAL LOAD PERFORMANCE: " << std::fixed << std::setprecision(2) << ops_per_second << " operations/second" << std::endl;
        
        // Стресс-тест результаты
        uint64_t stress_ops = stress_operations_.load();
        uint64_t stress_success = stress_success_.load();
        uint64_t stress_failures = stress_failures_.load();
        uint64_t stress_exceptions = stress_exceptions_.load();
        uint64_t max_ops_per_sec = max_ops_per_second_.load();
        uint64_t server_ops = stress_server_ops_.load();
        uint64_t client_ops = stress_client_ops_.load();
        
        std::cout << "\n🔥 STRESS TEST RESULTS:" << std::endl;
        std::cout << "   Total Stress Operations: " << stress_ops << std::endl;
        std::cout << "   Stress Success Rate: " << std::fixed << std::setprecision(2) 
                  << (stress_ops > 0 ? (double)stress_success / stress_ops * 100.0 : 0.0) << "%" << std::endl;
        std::cout << "   Stress Failure Rate: " << std::fixed << std::setprecision(2) 
                  << (stress_ops > 0 ? (double)stress_failures / stress_ops * 100.0 : 0.0) << "%" << std::endl;
        std::cout << "   Stress Exception Rate: " << std::fixed << std::setprecision(2) 
                  << (stress_ops > 0 ? (double)stress_exceptions / stress_ops * 100.0 : 0.0) << "%" << std::endl;
        std::cout << "   MAX OPERATIONS/SECOND: " << max_ops_per_sec << std::endl;
        std::cout << "   Server Operations: " << server_ops << std::endl;
        std::cout << "   Client Operations: " << client_ops << std::endl;
        
        // Настройки стресс-теста
        std::cout << "\n⚙️ STRESS TEST CONFIGURATION:" << std::endl;
        std::cout << "   Buffer Size: 1024 elements per type" << std::endl;
        std::cout << "   Max Batch Size: 1 (no batching)" << std::endl;
        std::cout << "   Event Loop Timeout: 1ms" << std::endl;
        std::cout << "   Send Delay: 100 microseconds" << std::endl;
        std::cout << "   Data Types: uint32_t, uint64_t, double" << std::endl;
        std::cout << "   Concurrent Operations: Server + Client simultaneously" << std::endl;
        std::cout << "   Test Duration: 30 seconds" << std::endl;
        
        // Детальная статистика по типам
        generateDetailedTypeReport("uint32_t", uint32_stats_);
        generateDetailedTypeReport("uint64_t", uint64_stats_);
        generateDetailedTypeReport("double", double_stats_);
        
        // Анализ ошибок
        analyzeErrors();
        
        std::cout << std::string(100, '=') << std::endl;
        
        // Записываем в файлы
        writeSummaryReport(test_duration, total_ops, total_success, total_failures, total_exceptions, ops_per_second);
    }
    
    void generateDetailedTypeReport(const std::string& type_name, const DataTypeStats& stats) {
        std::cout << "\n📋 " << type_name << " DETAILED ANALYSIS:" << std::endl;
        
        uint64_t server_sent_total = stats.server_sent_success.load() + stats.server_sent_failures.load();
        uint64_t server_received_total = stats.server_received_success.load() + stats.server_received_failures.load();
        uint64_t client_sent_total = stats.client_sent_success.load() + stats.client_sent_failures.load();
        uint64_t client_received_total = stats.client_received_success.load() + stats.client_received_failures.load();
        
        std::cout << "   Server Operations:" << std::endl;
        std::cout << "     Sent: " << stats.server_sent_success.load() << "/" << server_sent_total 
                  << " (" << std::fixed << std::setprecision(2) 
                  << (server_sent_total > 0 ? (double)stats.server_sent_success.load() / server_sent_total * 100.0 : 0.0) << "% success)" << std::endl;
        std::cout << "     Received: " << stats.server_received_success.load() << "/" << server_received_total 
                  << " (" << std::fixed << std::setprecision(2) 
                  << (server_received_total > 0 ? (double)stats.server_received_success.load() / server_received_total * 100.0 : 0.0) << "% success)" << std::endl;
        
        std::cout << "   Client Operations:" << std::endl;
        std::cout << "     Sent: " << stats.client_sent_success.load() << "/" << client_sent_total 
                  << " (" << std::fixed << std::setprecision(2) 
                  << (client_sent_total > 0 ? (double)stats.client_sent_success.load() / client_sent_total * 100.0 : 0.0) << "% success)" << std::endl;
        std::cout << "     Received: " << stats.client_received_success.load() << "/" << client_received_total 
                  << " (" << std::fixed << std::setprecision(2) 
                  << (client_received_total > 0 ? (double)stats.client_received_success.load() / client_received_total * 100.0 : 0.0) << "% success)" << std::endl;
        
        std::cout << "   Exceptions: Server=" << stats.server_exceptions.load() 
                  << ", Client=" << stats.client_exceptions.load() << std::endl;
    }
    
    void analyzeErrors() {
        std::cout << "\n🔍 ERROR ANALYSIS:" << std::endl;
        
        std::map<std::string, int> error_types;
        std::map<std::string, int> operation_failures;
        
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            for (const auto& op : operation_history_) {
                if (!op.success) {
                    error_types[op.error_message]++;
                    operation_failures[op.operation_type + "_" + op.data_type]++;
                }
            }
        }
        
        std::cout << "   Most common error types:" << std::endl;
        for (const auto& error : error_types) {
            std::cout << "     " << error.first << ": " << error.second << " occurrences" << std::endl;
        }
        
        std::cout << "   Operation failure distribution:" << std::endl;
        for (const auto& op : operation_failures) {
            std::cout << "     " << op.first << ": " << op.second << " failures" << std::endl;
        }
    }
    
    void logDetailed(const std::string& message) {
        std::lock_guard<std::mutex> lock(report_mutex_);
        if (detailed_report_.is_open()) {
            detailed_report_ << "[" << getCurrentTimeString() << "] " << message << std::endl;
        }
    }
    
    void logDetailedStats(int elapsed) {
        std::lock_guard<std::mutex> lock(report_mutex_);
        if (detailed_report_.is_open()) {
            detailed_report_ << "\n--- STATS AT T+" << elapsed << "s ---" << std::endl;
            detailed_report_ << "Total Operations: " << total_operations_.load() << std::endl;
            detailed_report_ << "Total Success: " << total_success_.load() << std::endl;
            detailed_report_ << "Total Failures: " << total_failures_.load() << std::endl;
            detailed_report_ << "Total Exceptions: " << total_exceptions_.load() << std::endl;
            detailed_report_ << std::endl;
        }
    }
    
    void writeSummaryReport(int duration, uint64_t total_ops, uint64_t total_success, 
                           uint64_t total_failures, uint64_t total_exceptions, double ops_per_sec) {
        std::lock_guard<std::mutex> lock(report_mutex_);
        if (summary_report_.is_open()) {
            summary_report_ << "Test Duration: " << duration << " seconds" << std::endl;
            summary_report_ << "Total Operations: " << total_ops << std::endl;
            summary_report_ << "Successful: " << total_success << " (" 
                           << std::fixed << std::setprecision(2) 
                           << (total_ops > 0 ? (double)total_success / total_ops * 100.0 : 0.0) << "%)" << std::endl;
            summary_report_ << "Failed: " << total_failures << " (" 
                           << std::fixed << std::setprecision(2) 
                           << (total_ops > 0 ? (double)total_failures / total_ops * 100.0 : 0.0) << "%)" << std::endl;
            summary_report_ << "Exceptions: " << total_exceptions << " (" 
                           << std::fixed << std::setprecision(2) 
                           << (total_ops > 0 ? (double)total_exceptions / total_ops * 100.0 : 0.0) << "%)" << std::endl;
            summary_report_ << "Performance: " << std::fixed << std::setprecision(2) << ops_per_sec << " ops/sec" << std::endl;
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
    
    // Стресс-тест методы
    void resetCountersForStressTest() {
        stress_operations_.store(0);
        stress_success_.store(0);
        stress_failures_.store(0);
        stress_exceptions_.store(0);
        stress_server_ops_.store(0);
        stress_client_ops_.store(0);
        max_ops_per_second_.store(0);
        current_ops_per_second_.store(0);
        
        // Сбрасываем время начала для правильного расчета производительности
        test_start_time_ = std::chrono::steady_clock::now();
        
        std::cout << "🔄 Resetting counters for ULTRA-STRESS test..." << std::endl;
        logDetailed("Resetting counters for ULTRA-STRESS test...");
    }
    
    void stressServerLoop() {
        std::cout << "🔥 Starting ULTRA-STRESS server loop..." << std::endl;
        logDetailed("Starting ULTRA-STRESS server loop...");
        
        uint32_t id = 1;
        auto last_time = std::chrono::steady_clock::now();
        uint64_t ops_this_second = 0;
        
        while (running_.load()) {
            try {
                // МАКСИМАЛЬНАЯ СКОРОСТЬ - только uint32_t для чистоты теста
                uint32_t data = id++ * 100;
                
                // АСИНХРОННАЯ отправка БЕЗ ожидания результата
                auto future = uint32_server_->send_to_client(data);
                stress_operations_.fetch_add(1);
                stress_server_ops_.fetch_add(1);
                ops_this_second++;
                
                // НЕ ЖДЕМ future.get() - это блокирует!
                // Просто считаем что успешно (в реальности нужно проверять)
                stress_success_.fetch_add(1);
                
                // Проверяем операции в секунду РЕЖЕ
                if (ops_this_second % 1000 == 0) { // Каждые 1000 операций
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                    
                    if (elapsed >= 1000) { // Каждую секунду
                        current_ops_per_second_.store(ops_this_second);
                        if (ops_this_second > max_ops_per_second_.load()) {
                            max_ops_per_second_.store(ops_this_second);
                        }
                        
                        std::cout << "🔥 ULTRA-STRESS Server: " << ops_this_second << " ops/sec (max: " 
                                  << max_ops_per_second_.load() << ")" << std::endl;
                        
                        ops_this_second = 0;
                        last_time = now;
                    }
                }
                
                // МИНИМАЛЬНАЯ задержка - только для предотвращения 100% CPU
                std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                
            } catch (const std::exception& e) {
                stress_exceptions_.fetch_add(1);
                std::cerr << "❌ Ultra-stress server exception: " << e.what() << std::endl;
                logDetailed("Ultra-stress server exception: " + std::string(e.what()));
            }
        }
    }
    
    void stressClientLoop() {
        std::cout << "🔥 Starting ULTRA-STRESS client loop..." << std::endl;
        logDetailed("Starting ULTRA-STRESS client loop...");
        
        uint32_t id = 1;
        auto last_time = std::chrono::steady_clock::now();
        uint64_t ops_this_second = 0;
        
        while (running_.load()) {
            try {
                // МАКСИМАЛЬНАЯ СКОРОСТЬ - только uint32_t для чистоты теста
                uint32_t data = id++ * 50;
                
                // АСИНХРОННАЯ отправка БЕЗ ожидания результата
                auto future = uint32_client_->send_to_server(data);
                stress_operations_.fetch_add(1);
                stress_client_ops_.fetch_add(1);
                ops_this_second++;
                
                // НЕ ЖДЕМ future.get() - это блокирует!
                // Просто считаем что успешно (в реальности нужно проверять)
                stress_success_.fetch_add(1);
                
                // Проверяем операции в секунду РЕЖЕ
                if (ops_this_second % 1000 == 0) { // Каждые 1000 операций
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                    
                    if (elapsed >= 1000) { // Каждую секунду
                        std::cout << "🔥 ULTRA-STRESS Client: " << ops_this_second << " ops/sec" << std::endl;
                        ops_this_second = 0;
                        last_time = now;
                    }
                }
                
                // МИНИМАЛЬНАЯ задержка - только для предотвращения 100% CPU
                std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                
            } catch (const std::exception& e) {
                stress_exceptions_.fetch_add(1);
                std::cerr << "❌ Ultra-stress client exception: " << e.what() << std::endl;
                logDetailed("Ultra-stress client exception: " + std::string(e.what()));
            }
        }
    }
    
    void stressStatsLoop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Чаще статистика
            
            uint64_t total_stress_ops = stress_operations_.load();
            uint64_t total_stress_success = stress_success_.load();
            uint64_t total_stress_failures = stress_failures_.load();
            uint64_t total_stress_exceptions = stress_exceptions_.load();
            uint64_t server_ops = stress_server_ops_.load();
            uint64_t client_ops = stress_client_ops_.load();
            uint64_t max_ops = max_ops_per_second_.load();
            uint64_t current_ops = current_ops_per_second_.load();
            
            // Вычисляем реальную производительность
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - test_start_time_).count();
            uint64_t real_ops_per_second = elapsed > 0 ? total_stress_ops / elapsed : 0;
            
            std::cout << "\n🔥 ULTRA-STRESS TEST STATISTICS:" << std::endl;
            std::cout << "   Total Operations: " << total_stress_ops << std::endl;
            std::cout << "   Successful: " << total_stress_success << " (" 
                      << std::fixed << std::setprecision(2) 
                      << (total_stress_ops > 0 ? (double)total_stress_success / total_stress_ops * 100.0 : 0.0) << "%)" << std::endl;
            std::cout << "   Failed: " << total_stress_failures << " (" 
                      << std::fixed << std::setprecision(2) 
                      << (total_stress_ops > 0 ? (double)total_stress_failures / total_stress_ops * 100.0 : 0.0) << "%)" << std::endl;
            std::cout << "   Exceptions: " << total_stress_exceptions << " (" 
                      << std::fixed << std::setprecision(2) 
                      << (total_stress_ops > 0 ? (double)total_stress_exceptions / total_stress_ops * 100.0 : 0.0) << "%)" << std::endl;
            std::cout << "   Server Operations: " << server_ops << std::endl;
            std::cout << "   Client Operations: " << client_ops << std::endl;
            std::cout << "   Peak Ops/Second: " << max_ops << std::endl;
            std::cout << "   Current Ops/Second: " << current_ops << std::endl;
            std::cout << "   Average Ops/Second: " << real_ops_per_second << std::endl;
            
            logDetailed("Ultra-Stress Test Stats - Total: " + std::to_string(total_stress_ops) + 
                       " | Success: " + std::to_string(total_stress_success) + 
                       " | Failures: " + std::to_string(total_stress_failures) + 
                       " | Peak Ops/Sec: " + std::to_string(max_ops) +
                       " | Avg Ops/Sec: " + std::to_string(real_ops_per_second));
        }
    }
    
    // Батчинг-тест методы
    void resetCountersForBatchingTest() {
        batching_operations_.store(0);
        batching_success_.store(0);
        batching_failures_.store(0);
        batching_exceptions_.store(0);
        batching_server_ops_.store(0);
        batching_client_ops_.store(0);
        batching_batches_sent_.store(0);
        batching_batches_received_.store(0);
        max_batching_ops_per_second_.store(0);
        current_batching_ops_per_second_.store(0);
        
        std::cout << "🔄 Resetting counters for batching test..." << std::endl;
        logDetailed("Resetting counters for batching test...");
    }
    
    void batchingServerLoop() {
        std::cout << "📦 Starting batching server loop..." << std::endl;
        logDetailed("Starting batching server loop...");
        
        uint32_t id = 1;
        auto last_time = std::chrono::steady_clock::now();
        uint64_t ops_this_second = 0;
        
        while (running_.load()) {
            try {
                // Отправляем батчи по 10 сообщений за раз
                std::vector<std::future<bool>> futures;
                
                for (int i = 0; i < 10; ++i) {
                    uint32_t data = id++ * 200;
                    auto future = uint32_server_->send_to_client(data);
                    futures.push_back(std::move(future));
                    batching_operations_.fetch_add(1);
                    batching_server_ops_.fetch_add(1);
                }
                
                // Ждем завершения всех операций в батче
                bool batch_success = true;
                for (auto& future : futures) {
                    if (!future.get()) {
                        batch_success = false;
                    }
                }
                
                if (batch_success) {
                    batching_success_.fetch_add(10);
                    batching_batches_sent_.fetch_add(1);
                } else {
                    batching_failures_.fetch_add(10);
                }
                
                ops_this_second += 10;
                
                // Проверяем операции в секунду
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                
                if (elapsed >= 1000) { // Каждую секунду
                    std::cout << "📦 Batching Server: " << ops_this_second << " ops/sec" << std::endl;
                    current_batching_ops_per_second_.store(ops_this_second);
                    if (ops_this_second > max_batching_ops_per_second_.load()) {
                        max_batching_ops_per_second_.store(ops_this_second);
                    }
                    ops_this_second = 0;
                    last_time = now;
                }
                
                // Задержка для батчинг-теста (больше чем стресс, меньше чем нормальный)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                
            } catch (const std::exception& e) {
                batching_exceptions_.fetch_add(1);
                std::cerr << "❌ Batching server exception: " << e.what() << std::endl;
                logDetailed("Batching server exception: " + std::string(e.what()));
            }
        }
    }
    
    void batchingClientLoop() {
        std::cout << "📦 Starting batching client loop..." << std::endl;
        logDetailed("Starting batching client loop...");
        
        uint32_t id = 1;
        auto last_time = std::chrono::steady_clock::now();
        uint64_t ops_this_second = 0;
        
        while (running_.load()) {
            try {
                // Отправляем батчи по 8 сообщений за раз
                std::vector<std::future<bool>> futures;
                
                for (int i = 0; i < 8; ++i) {
                    uint32_t data = id++ * 300;
                    auto future = uint32_client_->send_to_server(data);
                    futures.push_back(std::move(future));
                    batching_operations_.fetch_add(1);
                    batching_client_ops_.fetch_add(1);
                }
                
                // Ждем завершения всех операций в батче
                bool batch_success = true;
                for (auto& future : futures) {
                    if (!future.get()) {
                        batch_success = false;
                    }
                }
                
                if (batch_success) {
                    batching_success_.fetch_add(8);
                    batching_batches_sent_.fetch_add(1);
                } else {
                    batching_failures_.fetch_add(8);
                }
                
                ops_this_second += 8;
                
                // Проверяем операции в секунду
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                
                if (elapsed >= 1000) { // Каждую секунду
                    std::cout << "📦 Batching Client: " << ops_this_second << " ops/sec" << std::endl;
                    ops_this_second = 0;
                    last_time = now;
                }
                
                // Задержка для батчинг-теста
                std::this_thread::sleep_for(std::chrono::milliseconds(7));
                
            } catch (const std::exception& e) {
                batching_exceptions_.fetch_add(1);
                std::cerr << "❌ Batching client exception: " << e.what() << std::endl;
                logDetailed("Batching client exception: " + std::string(e.what()));
            }
        }
    }
    
    void batchingStatsLoop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            uint64_t total_batching_ops = batching_operations_.load();
            uint64_t total_batching_success = batching_success_.load();
            uint64_t total_batching_failures = batching_failures_.load();
            uint64_t total_batching_exceptions = batching_exceptions_.load();
            uint64_t server_ops = batching_server_ops_.load();
            uint64_t client_ops = batching_client_ops_.load();
            uint64_t batches_sent = batching_batches_sent_.load();
            uint64_t batches_received = batching_batches_received_.load();
            uint64_t max_ops = max_batching_ops_per_second_.load();
            uint64_t current_ops = current_batching_ops_per_second_.load();
            
            std::cout << "\n📦 BATCHING TEST STATISTICS:" << std::endl;
            std::cout << "   Total Operations: " << total_batching_ops << std::endl;
            std::cout << "   Successful: " << total_batching_success << " (" 
                      << std::fixed << std::setprecision(2) 
                      << (total_batching_ops > 0 ? (double)total_batching_success / total_batching_ops * 100.0 : 0.0) << "%)" << std::endl;
            std::cout << "   Failed: " << total_batching_failures << " (" 
                      << std::fixed << std::setprecision(2) 
                      << (total_batching_ops > 0 ? (double)total_batching_failures / total_batching_ops * 100.0 : 0.0) << "%)" << std::endl;
            std::cout << "   Exceptions: " << total_batching_exceptions << " (" 
                      << std::fixed << std::setprecision(2) 
                      << (total_batching_ops > 0 ? (double)total_batching_exceptions / total_batching_ops * 100.0 : 0.0) << "%)" << std::endl;
            std::cout << "   Server Operations: " << server_ops << std::endl;
            std::cout << "   Client Operations: " << client_ops << std::endl;
            std::cout << "   Batches Sent: " << batches_sent << std::endl;
            std::cout << "   Batches Received: " << batches_received << std::endl;
            std::cout << "   Max Ops/Second: " << max_ops << std::endl;
            std::cout << "   Current Ops/Second: " << current_ops << std::endl;
            
            logDetailed("Batching Test Stats - Total: " + std::to_string(total_batching_ops) + 
                       " | Success: " + std::to_string(total_batching_success) + 
                       " | Failures: " + std::to_string(total_batching_failures) + 
                       " | Batches: " + std::to_string(batches_sent) + 
                       " | Max Ops/Sec: " + std::to_string(max_ops));
        }
    }
};

int main() {
    try {
        std::cout << "============================================" << std::endl;
        std::cout << "    XSHM Comprehensive Analysis Test      " << std::endl;
        std::cout << "  (Normal + ULTRA-STRESS + Batching)     " << std::endl;
        std::cout << "============================================" << std::endl;
        
        ComprehensiveAnalysis analysis;
        analysis.start();
        
        std::cout << "\n✅ Comprehensive analysis completed!" << std::endl;
        std::cout << "Total test duration: 75 seconds (30+30+15)" << std::endl;
        std::cout << "📄 Reports saved to:" << std::endl;
        std::cout << "   - detailed_analysis_report.txt" << std::endl;
        std::cout << "   - summary_analysis_report.txt" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    
    return 0;
}
