# XSHM Test Suite

Комплексный набор тестов для библиотеки XSHM (eXtended Shared Memory) с поддержкой различных режимов работы и детальной аналитикой производительности.

## 🚀 Обзор

Библиотека XSHM предоставляет высокопроизводительный механизм межпроцессного взаимодействия через shared memory с поддержкой:
- **Lock-free ring buffers** для максимальной производительности
- **Асинхронного и синхронного** режимов работы
- **Автоматического переподключения** при сбоях
- **Batch processing** для групповой обработки данных
- **Детальной статистики** производительности

## 📁 Структура тестов

### Основные тесты

| Файл | Описание | Режим | Длительность |
|------|----------|-------|--------------|
| `background_stress_test.cpp` | Стресс-тест с детальной статистикой | ASYNC/SYNC | 30 сек |
| `comprehensive_mode_test.cpp` | Сравнительный анализ режимов | ASYNC + SYNC | 60 сек |
| `silent_stress_test.cpp` | Максимально быстрый тест | ASYNC | 30 сек |
| `comprehensive_analysis.cpp` | Полный анализ с 3 фазами | Mixed | 75 сек |

### Скрипты сборки и запуска

| Скрипт | Назначение |
|--------|------------|
| `run_all_mode_tests.bat` | Интерактивный запуск всех тестов |
| `build_comprehensive_test.bat` | Сборка comprehensive_analysis |
| `run_comprehensive_test.bat` | Запуск comprehensive_analysis |

## 🔧 Сборка тестов

### Автоматическая сборка
```bash
# Запуск интерактивного меню
run_all_mode_tests.bat
```

### Ручная сборка
```bash
# Background stress test (поддерживает оба режима)
bcc64x background_stress_test.cpp ..\xshm.obj -o background_stress_async.exe

# Comprehensive mode test (сравнение режимов)
bcc64x comprehensive_mode_test.cpp ..\xshm.obj -o comprehensive_mode_test.exe

# Silent stress test (максимальная скорость)
bcc64x silent_stress_test.cpp ..\xshm.obj -o silent_stress_test.exe

# Comprehensive analysis (3-фазный тест)
bcc64x comprehensive_analysis.cpp ..\xshm.obj -o comprehensive_analysis.exe
```

## 🎯 Режимы работы

### ASYNC Mode (Асинхронный)
- **Отправка**: Без ожидания подтверждения
- **Производительность**: Максимальная (~126 ops/sec)
- **Надежность**: ~53% успешных операций
- **Использование**: Высокоскоростная передача данных

```cpp
// Пример использования
auto future = api->send_to_client(data);
// НЕ ждем future.get() - максимальная скорость
```

### SYNC Mode (Синхронный)
- **Отправка**: С ожиданием подтверждения
- **Производительность**: Сниженная (~80-100 ops/sec)
- **Надежность**: ~95-100% успешных операций
- **Использование**: Критически важные данные

```cpp
// Пример использования
auto future = api->send_to_client(data);
if (future.get()) {
    // Данные успешно доставлены
}
```

## 📊 Метрики производительности

### Основные показатели
- **Total Operations**: Общее количество отправленных операций
- **Successful**: Успешно доставленные сообщения
- **Failed**: Неудачные операции
- **Success Rate**: Процент успешных операций
- **Ops/Second**: Операций в секунду (пиковая/средняя)

### Детальная аналитика
- **Async Sent/Received**: Асинхронные операции
- **Sync Sent/Received**: Синхронные операции
- **Server/Client Operations**: Операции по сторонам
- **Peak Performance**: Пиковая производительность
- **Average Performance**: Средняя производительность

## 🧪 Типы тестов

### 1. Background Stress Test
**Файл**: `background_stress_test.cpp`
**Режимы**: ASYNC (по умолчанию), SYNC (с флагом `--sync`)

```bash
# Асинхронный режим
background_stress_async.exe

# Синхронный режим
background_stress_async.exe --sync
```

**Особенности**:
- Детальная статистика каждые 5 секунд
- Раздельный подсчет async/sync операций
- Логирование в файл `background_stress_results.txt`
- Минимальная задержка (1 наносекунда)

### 2. Comprehensive Mode Test
**Файл**: `comprehensive_mode_test.cpp`
**Режимы**: Автоматическое сравнение ASYNC vs SYNC

```bash
comprehensive_mode_test.exe
```

**Особенности**:
- Последовательный запуск обоих режимов
- Сравнительный анализ производительности
- Рекомендации по выбору режима
- Отчет в `comprehensive_mode_report.txt`

### 3. Silent Stress Test
**Файл**: `silent_stress_test.cpp`
**Режим**: ASYNC (максимальная скорость)

```bash
silent_stress_test.exe
```

**Особенности**:
- БЕЗ консольного вывода во время теста
- БЕЗ обработчиков событий
- Только отправка данных
- Максимальная производительность

### 4. Comprehensive Analysis
**Файл**: `comprehensive_analysis.cpp`
**Режимы**: 3-фазный тест

```bash
comprehensive_analysis.exe
```

**Фазы**:
1. **Normal Load** (30 сек): Обычная нагрузка
2. **ULTRA-STRESS** (30 сек): Максимальная нагрузка
3. **Batching** (15 сек): Групповая обработка

## 📈 Анализ результатов

### Типичные результаты ASYNC режима
```
Mode: ASYNC (no confirmation)
Total Operations: 3787
Successful: 1997 (52.73%)
Failed: 0 (0.00%)
Average Ops/Second: 126
```

### Типичные результаты SYNC режима
```
Mode: SYNC (with confirmation)
Total Operations: 2400
Successful: 2280 (95.00%)
Failed: 120 (5.00%)
Average Ops/Second: 80
```

### Сравнительный анализ
```
Performance Comparison:
  ASYNC: 126 ops/sec
  SYNC:  80 ops/sec
  Ratio: 1.58x (ASYNC vs SYNC)

Reliability Comparison:
  ASYNC Success Rate: 52.73%
  SYNC Success Rate:  95.00%
```

## ⚙️ Конфигурация

### XSHMConfig параметры
```cpp
xshm::XSHMConfig config;
config.enable_logging = false;           // Отключить логирование
config.enable_auto_reconnect = true;     // Автопереподключение
config.event_loop_timeout_ms = 0;        // Минимальный таймаут
config.max_batch_size = 1;               // Размер батча
config.callback_thread_pool_size = 20;   // Размер пула потоков
```

### Оптимизация производительности
- **Увеличить размер буфера**: `1024` → `4096` или `8192`
- **Включить batch processing**: `max_batch_size = 10`
- **Настроить пул потоков**: `callback_thread_pool_size = 50`
- **Отключить логирование**: `enable_logging = false`

## 🔍 Диагностика проблем

### Низкая производительность
1. Проверить размер буфера
2. Включить batch processing
3. Отключить консольный вывод
4. Увеличить размер пула потоков

### Высокий процент ошибок
1. Переключиться на SYNC режим
2. Увеличить таймауты
3. Проверить стабильность соединения
4. Включить автопереподключение

### Проблемы с памятью
1. Уменьшить размер буфера
2. Проверить доступную память
3. Мониторить использование shared memory

## 📋 Рекомендации по использованию

### Выбор режима
- **ASYNC**: Высокоскоростная передача, допустимы потери
- **SYNC**: Критически важные данные, надежность важнее скорости

### Оптимальные настройки
- **Для максимальной скорости**: ASYNC + batch processing
- **Для надежности**: SYNC + автопереподключение
- **Для баланса**: ASYNC с мониторингом ошибок

### Мониторинг
- Регулярно проверяйте success rate
- Мониторьте пиковую производительность
- Анализируйте паттерны ошибок

## 🚨 Известные ограничения

1. **Windows only**: Библиотека работает только на Windows
2. **Trivial types**: Поддерживаются только простые типы данных
3. **Memory limits**: Ограничения размера shared memory
4. **Process isolation**: Требуются отдельные процессы для server/client

## 📞 Поддержка

При возникновении проблем:
1. Проверьте логи в файлах результатов
2. Запустите диагностические тесты
3. Проанализируйте метрики производительности
4. Обратитесь к документации библиотеки XSHM

---

**Версия тестов**: 2.0.0  
**Совместимость**: XSHM Library v1.0+  
**Платформа**: Windows x64  
**Компилятор**: Embarcadero C++ 7.80+