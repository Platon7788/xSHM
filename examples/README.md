# XSHM Examples

Этот каталог содержит примеры использования библиотеки XSHM v5.0.0 для различных сценариев.

## 📁 Структура примеров

### 🚀 Базовые примеры
- **`simple_server.cpp`** - Простой сервер с базовой функциональностью
- **`simple_client.cpp`** - Простой клиент с базовой функциональностью

### 💬 Продвинутые примеры
- **`chat_server.cpp`** - Сервер чата с обработкой сообщений
- **`chat_client.cpp`** - Клиент чата с интерактивным вводом
- **`monitoring_server.cpp`** - Сервер мониторинга системы
- **`monitoring_client.cpp`** - Клиент мониторинга системы

### 🛡️ Безопасные примеры
- **`safe_reading_server.cpp`** - Сервер с безопасным чтением данных
- **`safe_reading_client.cpp`** - Клиент с безопасным чтением данных

### ⚙️ Конфигурации
- **`balanced_config.cpp`** - Сбалансированная конфигурация для среднего ПК
- **`extreme_performance_config.cpp`** - Конфигурация для максимальной производительности
- **`realtime_config.cpp`** - Конфигурация для систем реального времени

### 🔧 Специальные примеры
- **`batching_demo.cpp`** - Демонстрация batch processing
- **`xshmessage_example.cpp`** - Пример использования XSHMessage для произвольных данных

## 🎯 Описание примеров

### Базовые примеры

#### Simple Server/Client
**Файлы**: `simple_server.cpp`, `simple_client.cpp`

Простейший пример использования XSHM с типом `uint32_t`:
- ✅ Базовая конфигурация
- ✅ Обработчики подключения и данных
- ✅ Интерактивная отправка данных
- ✅ Автоматическое управление ресурсами

```cpp
// Создание сервера
auto server = xshm::AsyncXSHM<uint32_t>::create_server("simple_app", 1024, config);

// Обработчик данных от клиентов
on_data_received_cxs(server, [](const uint32_t* data) {
    if (data) {
        std::cout << "Server received: " << *data << std::endl;
    }
});

// Отправка данных
server->send_to_client(counter++);
```

### Продвинутые примеры

#### Chat Server/Client
**Файлы**: `chat_server.cpp`, `chat_client.cpp`

Пример чата с расширенной функциональностью:
- ✅ Оптимизированная конфигурация для чата
- ✅ Batch processing для групповых сообщений
- ✅ Обработка сообщений с ID
- ✅ Статистика отправки/получения

#### Monitoring Server/Client
**Файлы**: `monitoring_server.cpp`, `monitoring_client.cpp`

Система мониторинга с метриками:
- ✅ Генерация системных метрик
- ✅ Периодическая отправка данных
- ✅ Статистика в реальном времени
- ✅ Многопоточная обработка

### Безопасные примеры

#### Safe Reading Server/Client
**Файлы**: `safe_reading_server.cpp`, `safe_reading_client.cpp`

Демонстрация безопасного чтения с verification:
- ✅ Sequence verification для целостности данных
- ✅ Checksum проверка данных
- ✅ Ручное безопасное чтение из буферов
- ✅ Демонстрация RAII и HandleGuard

```cpp
// Безопасное чтение с sequence verification
uint64_t sequence;
MyData* data = buffer.try_read(sequence);
if (data) {
    // Обработка данных
    if (buffer.commit_read(sequence)) {
        std::cout << "Data safely processed" << std::endl;
    }
}
```

### Конфигурации

#### Balanced Config
**Файл**: `balanced_config.cpp`

Оптимальная конфигурация для среднего ПК (4-8 ядер, 8-32 ГБ RAM):
- ✅ Сбалансированная производительность
- ✅ Надежность и стабильность
- ✅ Оптимальные таймауты
- ✅ Подходящие размеры буферов

#### Extreme Performance Config
**Файл**: `extreme_performance_config.cpp`

Конфигурация для максимальной производительности:
- ✅ Максимальные размеры буферов
- ✅ Большой thread pool
- ✅ Агрессивные настройки
- ✅ Минимальные задержки

#### Realtime Config
**Файл**: `realtime_config.cpp`

Конфигурация для систем реального времени:
- ✅ Минимальные таймауты
- ✅ Немедленная обработка
- ✅ Приоритет скорости над надежностью
- ✅ Оптимизация для low-latency

### Специальные примеры

#### Batching Demo
**Файл**: `batching_demo.cpp`

Демонстрация batch processing:
- ✅ Настройка размера батча
- ✅ Групповая обработка сообщений
- ✅ Оптимизация производительности
- ✅ Визуализация работы батчинга

## 📊 Структуры данных

### Простые типы
```cpp
// Базовые типы (автоматически поддерживаются)
xshm::AsyncXSHM<uint32_t>     // unsigned int
xshm::AsyncXSHM<uint64_t>     // unsigned long long
xshm::AsyncXSHM<double>       // double
xshm::AsyncXSHM<char>         // char
```

### Пользовательские структуры
```cpp
// ChatMessage (для чата)
struct ChatMessage {
    uint64_t id;
    char sender[32];
    char text[256];
    uint64_t timestamp;
};

// SystemMetrics (для мониторинга)
struct SystemMetrics {
    uint64_t timestamp;
    double cpu_usage;
    double memory_usage;
    uint32_t process_count;
    uint64_t network_bytes;
    char hostname[64];
};

// SecureData (для безопасного чтения)
struct SecureData {
    uint64_t id;
    char message[128];
    uint32_t checksum;
    uint64_t timestamp;
};
```

## 🔧 Как использовать примеры

### Компиляция
```bash
# Компиляция библиотеки
bcc64x -c ../xshm.cpp -o ../xshm.obj

# Компиляция примера
bcc64x simple_server.cpp ../xshm.obj -o simple_server.exe
bcc64x simple_client.cpp ../xshm.obj -o simple_client.exe
```

### Запуск
```bash
# Терминал 1: Запуск сервера
./simple_server.exe

# Терминал 2: Запуск клиента
./simple_client.exe
```

## 🎯 Рекомендации по выбору примера

### Для изучения основ
- **Начните с**: `simple_server.cpp` и `simple_client.cpp`
- **Изучите**: Базовые концепции XSHM

### Для практических задач
- **Чат**: `chat_server.cpp` и `chat_client.cpp`
- **Мониторинг**: `monitoring_server.cpp` и `monitoring_client.cpp`
- **Безопасность**: `safe_reading_server.cpp` и `safe_reading_client.cpp`

### Для оптимизации
- **Сбалансированная производительность**: `balanced_config.cpp`
- **Максимальная скорость**: `extreme_performance_config.cpp`
- **Реальное время**: `realtime_config.cpp`
- **Batch processing**: `batching_demo.cpp`

## 🛡️ Безопасность и лучшие практики

Все примеры демонстрируют:
- ✅ **RAII управление ресурсами** - автоматическое закрытие handle'ов
- ✅ **Sequence verification** - безопасное чтение данных
- ✅ **Exception safety** - корректная обработка ошибок
- ✅ **Input validation** - проверка входных данных
- ✅ **Memory safety** - отсутствие утечек памяти

### ✅ Рекомендуется
- Используйте `server->send_to_client()` и `client->send_to_server()` для отправки
- Используйте `on_data_received_cxs()` и `on_data_received_sxc()` для приема
- Проверяйте `data` на `nullptr` в обработчиках
- Настройте конфигурацию под ваши нужды
- Используйте sequence verification для критичных данных

### ❌ Избегайте
- Не используйте `std::string` в структурах данных
- Не забывайте проверять `data` в обработчиках
- Не игнорируйте deprecation warnings
- Не создавайте слишком много серверов с одинаковыми именами

## 🔍 Отладка и мониторинг

### Логирование
```cpp
xshm::XSHMConfig config;
config.enable_logging = true;  // Включить для отладки
```

### Статистика
```cpp
auto stats = server->get_statistics();
std::cout << "Writes: " << stats.sxc_writes << std::endl;
std::cout << "Reads: " << stats.cxs_reads << std::endl;
```

### Мониторинг производительности
```cpp
config.enable_statistics = true;
config.enable_activity_tracking = true;
config.enable_performance_counters = true;
```

## 🚀 XSHMessage - Удобная обертка для произвольных данных

### XSHMessage Example
**Файл**: `xshmessage_example.cpp`

**XSHMessage** - это удобная обертка над XSHM, которая скрывает все ограничения библиотеки и позволяет отправлять **любые данные**:

- ✅ **Произвольные данные** - `std::vector<uint8_t>`, `std::string`, `void*`
- ✅ **Простой API** - всего несколько методов
- ✅ **Автоматическая сборка** - сообщения собираются автоматически
- ✅ **Совместимость** - работает с существующим кодом

```cpp
// Создание конфигурации
xshm::XSHMConfig config;
config.enable_logging = true;
config.enable_auto_reconnect = true;
config.enable_statistics = true;
config.event_loop_timeout_ms = 0;  // Реальное время
config.max_batch_size = 1;         // Без батчинга

// Создание сервера с конфигурацией
auto server = xshm::XSHMessage::create_server("my_service", config);

// Обработчик сообщений
server->on_message([](const std::vector<uint8_t>& data) {
    std::cout << "Received " << data.size() << " bytes" << std::endl;
});

// Создание клиента с конфигурацией
auto client = xshm::XSHMessage::connect("my_service", config);

// Отправка любых данных
std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03};
client->send(binary_data);

std::string text = "Hello World!";
client->send(text);

const char* raw_data = "Raw data";
client->send(raw_data, strlen(raw_data));
```

### Компиляция и запуск
```bash
# Компиляция
build_xshmessage_example.bat

# Запуск
xshmessage_example.exe
```

## 📚 Дополнительная информация

- [Основной README](../README.md) - Полная документация библиотеки
- [Тестовая система](../test/README.md) - Комплексные тесты производительности
- [xshm.hpp](../xshm.hpp) - API документация с Doxygen

## 🎉 Готовность к продакшену

Все примеры готовы для использования в production:
- ✅ **Enterprise-grade качество** - production-ready код
- ✅ **Thread-safe архитектура** - безопасная работа в многопоточной среде
- ✅ **Comprehensive error handling** - обработка ошибок во всех операциях
- ✅ **Memory safety** - отсутствие утечек памяти
- ✅ **Configurable behavior** - настройка под различные нагрузки

---

*Версия: XSHM v5.0.0*  
*Платформа: Windows x64*  
*Статус: Enterprise Production Ready* ✅