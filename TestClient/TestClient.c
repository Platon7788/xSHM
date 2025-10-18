#pragma warning(disable: 4996)
#pragma warning(disable: 4273)

#include "../include/shm_ipc.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <stdlib.h>  // Для qsort

// Используем константы из shm_types.h
#define SHUTDOWN_FLAG 0x1234

typedef struct {
    uint32_t id;
    uint32_t type; // 0 - ping, 1 - pong
    double timestamp; // Время отправки текущего сообщения
    double original_timestamp; // Для pong: время отправки оригинального ping (round-trip)
    char message[244]; // Уменьшено, чтобы общий размер был 268 байт (4+4+8+8+244)
} test_message_t;

static volatile int shutdown_requested = 0;
static volatile int test_done = 0; // Сигнал о завершении теста

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        shutdown_requested = SHUTDOWN_FLAG;
        printf("\nShutdown requested. Stopping...\n");
        return TRUE;
    }
    return FALSE;
}

// --- НОВОЕ: Обработчик событий клиента ---
void client_event_handler(void* user_data, shm_event_type_t event_type, const void* data, uint32_t size) {
    (void)user_data; (void)data; (void)size; // Используем size, если нужно
    switch (event_type) {
    case SHM_EVENT_CONNECT:
        // printf("Client: Received CONNECT event (this should not happen for client).\n");
        // Клиент не должен получать CONNECT, так как он сам инициирует подключение
        break;
    case SHM_EVENT_DISCONNECT:
        printf("Client: Received DISCONNECT event from server.\n");
        test_done = 1; // Клиент завершает тест при отключении сервера
        break;
    case SHM_EVENT_DATA_AVAILABLE:
        // printf("Client: Received DATA_AVAILABLE event.\n"); // Может быть много
        break;
    case SHM_EVENT_ERROR:
        printf("Client: Received ERROR event.\n");
        break;
    default:
        printf("Client: Received unknown event type: %d\n", event_type);
        break;
    }
}
// ----------------------------------------

// Comparator for qsort (for doubles) - исправлен для совместимости
static int compare_doubles(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    else if (da > db) return 1;
    else return 0;
}

int main(void) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    printf("=== SHM IPC Client Bi-Dir Stress Test (%u msgs each way) ===\n", SHM_TEST_NUM_MESSAGES);

    shm_client_t* client = shm_client_connect("test_channel");
    if (!client) {
        printf("Connect failed.\n");
        return 1;
    }
    printf("Connected.\n");

    // Регистрируем обработчик событий - слушатель запустится автоматически
    shm_error_t reg_result = shm_client_register_callback(client, client_event_handler, (void*)"test_channel");
    if (reg_result != SHM_SUCCESS) {
        printf("Client: Failed to register callback: %d\n", reg_result);
        shm_client_disconnect(client);
        return 1;
    }

    // Timestamp for files
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);

    char report_file[128];
    snprintf(report_file, sizeof(report_file), "client_report_%s.txt", timestamp);
    char csv_file[128];
    snprintf(csv_file, sizeof(csv_file), "client_report_%s.csv", timestamp);

    uint64_t start_time = GetTickCount64();
    uint32_t sent_count = 0, recv_count = 0, error_count = 0, full_count = 0, out_of_order_loss = 0, dropped_count = 0;
    double total_latency = 0.0, total_send_latency = 0.0, total_recv_latency = 0.0;
    uint32_t expected_pong_id = 0; // Ожидаемые ID для pong-сообщений (ответов на клиентские ping)
    uint32_t expected_server_ping_id = 0; // Ожидаемые ID для ping-сообщений от сервера
    uint64_t total_data_bytes = 0;
    uint32_t progress = 0, total_retries = 0, max_retries = 0;
    double prev_interval_time = (double)start_time / 1000.0;
    uint64_t interval_data = 0, interval_sent = 0, interval_recv = 0;
    double peak_throughput = 0.0, running_loss_rate = 0.0;
    double latencies[100] = { 0 };  // Sample for median
    uint32_t latency_samples = 0;
    double max_buffer_usage = 0.0;

    // --- ИСПРАВЛЕНО: Отдельные счётчики для направлений ---
    uint32_t sent_pings = 0; // Количество отправленных ping от клиента
    uint32_t recv_pongs = 0; // Количество полученных pong на клиентские ping
    uint32_t recv_server_pings = 0; // Количество полученных ping от сервера
    uint32_t sent_pongs = 0; // Количество отправленных pong на серверные ping
    uint32_t out_of_order_pongs = 0; // Out-of-order для pong
    uint32_t out_of_order_server_pings = 0; // Out-of-order для server ping
    // ----------------------------------------------------------------

    // Цикл до выполнения всех итераций или таймаута или отключения
    uint32_t sent_this_side = 0;
    uint32_t recv_this_side = 0;
    while ((sent_this_side < 2 * SHM_TEST_NUM_MESSAGES || recv_this_side < 2 * SHM_TEST_NUM_MESSAGES) &&
        !shutdown_requested &&
        !test_done && // test_done устанавливается обработчиком событий клиента
        (GetTickCount64() - start_time) / 1000 <= SHM_TEST_MAX_DURATION_SEC) {

        // Отправка ping (запрос от клиента)
        if (sent_pings < SHM_TEST_NUM_MESSAGES) {
            uint64_t send_start = GetTickCount64();
            test_message_t msg = { 0 };
            msg.id = sent_pings;
            msg.type = 0; // Ping
            msg.timestamp = (double)GetTickCount64() / 1000.0;
            msg.original_timestamp = 0.0; // Не используется для ping
            snprintf(msg.message, sizeof(msg.message), "Client Ping #%u", sent_pings);

            // --- ОБНОВЛЕНО: shm_client_send всегда успешен для корректного размера ---
            shm_error_t send_result = shm_client_send(client, &msg, sizeof(msg)); // Вызов напрямую
            total_send_latency += (GetTickCount64() - send_start) / 1000.0;

            if (send_result == SHM_SUCCESS) {
                sent_count++;
                sent_pings++;
                sent_this_side++;
                total_data_bytes += sizeof(msg);
                interval_sent++;
                interval_data += sizeof(msg);
            }
            else { // Остальные ошибки
                error_count++;
                dropped_count++;
            }
        }

        // Приём pong (ответ от сервера на клиентский ping) ИЛИ ping (запрос от сервера)
        uint64_t recv_start = GetTickCount64();
        uint8_t recv_buffer[1024];
        uint32_t recv_size = sizeof(recv_buffer);
        shm_error_t receive_result = shm_client_receive(client, recv_buffer, &recv_size);
        total_recv_latency += (GetTickCount64() - recv_start) / 1000.0;

        if (receive_result == SHM_SUCCESS) {
            test_message_t* recv_msg = (test_message_t*)recv_buffer;
            total_data_bytes += recv_size;

            if (recv_msg->type == 1) { // Pong (ответ на клиентский ping)
                // --- ИСПРАВЛЕНО: Round-trip latency с original_timestamp ---
                double msg_latency = ((double)GetTickCount64() / 1000.0 - recv_msg->original_timestamp);
                // ----------------------------------------------------------------
                if (latency_samples < 100) {
                    latencies[latency_samples++] = msg_latency * 1000.0; // ms
                }
                if (recv_msg->id == expected_pong_id) {
                    recv_count++;
                    recv_pongs++;
                    recv_this_side++;
                    total_latency += msg_latency;
                    interval_recv++;
                    expected_pong_id++;
                }
                else if (recv_msg->id > expected_pong_id) {
                    out_of_order_pongs += (recv_msg->id - expected_pong_id);
                    expected_pong_id = recv_msg->id + 1; // Обновляем ожидаемый ID
                    recv_count++;
                    recv_pongs++;
                    recv_this_side++;
                    total_latency += msg_latency;
                    interval_recv++;
                }
                // else: recv_msg->id < expected_pong_id -> дубликат, игнорируем
            }
            else if (recv_msg->type == 0) { // Ping (запрос от сервера)
                recv_count++;
                recv_server_pings++;
                recv_this_side++;
                interval_recv++;
                if (recv_msg->id == expected_server_ping_id) {
                    expected_server_ping_id++;
                }
                else if (recv_msg->id > expected_server_ping_id) {
                    out_of_order_server_pings += (recv_msg->id - expected_server_ping_id);
                    expected_server_ping_id = recv_msg->id + 1;
                }
                // Отправка ответа (Pong) на ping от сервера
                uint64_t send_start_pong = GetTickCount64();
                test_message_t response = { 0 };
                response.id = recv_msg->id; // Отвечаем с тем же ID
                response.type = 1; // Pong
                response.timestamp = (double)GetTickCount64() / 1000.0;
                response.original_timestamp = recv_msg->timestamp; // Копируем для round-trip
                snprintf(response.message, sizeof(response.message), "Client Pong #%u", recv_msg->id);

                // --- ОБНОВЛЕНО: shm_client_send всегда успешен для корректного размера ---
                shm_error_t send_result = shm_client_send(client, &response, sizeof(response)); // Вызов напрямую
                total_send_latency += (GetTickCount64() - send_start_pong) / 1000.0;

                if (send_result == SHM_SUCCESS) {
                    sent_count++;
                    sent_pongs++;
                    sent_this_side++;
                    total_data_bytes += sizeof(response);
                    interval_sent++;
                    interval_data += sizeof(response);
                }
                else { // Остальные ошибки
                    error_count++;
                    dropped_count++;
                }
            }
            interval_data += recv_size;
        }
        else if (receive_result != SHM_ERROR_EMPTY) {
            error_count++;
            dropped_count++;
        }

        Sleep(0); // Yield после итерации

        // Progress & running metrics
        if (++progress % SHM_TEST_PROGRESS_INTERVAL == 0) {
            double curr_time = (double)GetTickCount64() / 1000.0;
            double interval_sec = curr_time - prev_interval_time;
            if (interval_sec < 0.001) interval_sec = 0.001; // Avoid div by zero
            double interval_throughput = interval_data / (1024.0 * 1024.0) / interval_sec;
            if (interval_throughput > peak_throughput) peak_throughput = interval_throughput;
            uint32_t total_out_of_order = out_of_order_pongs + out_of_order_server_pings;
            uint32_t total_lost = (SHM_TEST_NUM_MESSAGES - recv_pongs + out_of_order_pongs) + (SHM_TEST_NUM_MESSAGES - recv_server_pings + out_of_order_server_pings);
            running_loss_rate = (double)total_lost / (progress * 2) * 100.0; // Примерно *2 за направления
            // --- ИСПРАВЛЕНО: Используем buffer_size rx_ring ---
            double buffer_usage = (double)shm_ring_available(client->rx_ring) / (double)client->rx_ring->buffer_size * 100.0;
            if (buffer_usage > max_buffer_usage) max_buffer_usage = buffer_usage;
            printf("Progress: Sent %u/%u, Recv %u/%u (Throughput: %.1f MB/s | Loss: %.1f%% | Buffer: %.1f%%)\n",
                sent_this_side, 2 * SHM_TEST_NUM_MESSAGES, recv_this_side, 2 * SHM_TEST_NUM_MESSAGES,
                interval_throughput, running_loss_rate, buffer_usage);
            prev_interval_time = curr_time;
            interval_data = 0;
            interval_sent = 0;
            interval_recv = 0;
        }
    }

    uint64_t end_time = GetTickCount64();
    double duration_sec = (end_time - start_time) / 1000.0;

    // --- ИСПРАВЛЕНО: Расчёт потерь ---
    uint32_t lost_pongs = SHM_TEST_NUM_MESSAGES - recv_pongs + out_of_order_pongs;
    uint32_t lost_server_pings = SHM_TEST_NUM_MESSAGES - recv_server_pings + out_of_order_server_pings;
    uint32_t total_lost = lost_pongs + lost_server_pings;
    // ---------------------------------

    // Проверка завершения
    if (sent_pings >= SHM_TEST_NUM_MESSAGES && recv_pongs >= SHM_TEST_NUM_MESSAGES &&
        recv_server_pings >= SHM_TEST_NUM_MESSAGES && sent_pongs >= SHM_TEST_NUM_MESSAGES) {
        printf("Client finished successfully: Sent %u, Received %u.\n", sent_count, recv_count);
    }
    else if (test_done) {
        printf("Client finished due to server disconnect. Sent %u/%u, Received %u/%u.\n", sent_count, 2 * SHM_TEST_NUM_MESSAGES, recv_count, 2 * SHM_TEST_NUM_MESSAGES);
    }
    else {
        printf("Client finished prematurely. Sent %u/%u, Received %u/%u.\n", sent_count, 2 * SHM_TEST_NUM_MESSAGES, recv_count, 2 * SHM_TEST_NUM_MESSAGES);
    }

    // // Median latency (qsort)
    double median_latency = 0.0;
    if (latency_samples > 0) {
        qsort(latencies, latency_samples, sizeof(double), compare_doubles);
        median_latency = latencies[latency_samples / 2];
    }

    // Расчёты
    double send_rate = (double)sent_count / duration_sec;
    double recv_rate = (double)recv_count / duration_sec;
    double total_rate = (send_rate + recv_rate) / 2.0;
    double send_success = (double)sent_count / (2.0 * SHM_TEST_NUM_MESSAGES) * 100.0; // Успех относительно 2N отправок
    double recv_success = (double)recv_count / (2.0 * SHM_TEST_NUM_MESSAGES) * 100.0; // Успех относительно 2N приёмов
    double overall_success = ((double)(sent_count + recv_count) / (4.0 * SHM_TEST_NUM_MESSAGES)) * 100.0; // Успех относительно общего числа возможных сообщений
    double loss_rate = (double)total_lost / (2.0 * SHM_TEST_NUM_MESSAGES) * 100.0; // Общая потеря
    double avg_latency_ms = (recv_pongs > 0) ? total_latency / recv_pongs * 1000.0 : 0.0;
    double avg_send_latency_ms = sent_count > 0 ? total_send_latency / sent_count * 1000.0 : 0.0; // Усреднено по всем отправкам
    double avg_recv_latency_ms = recv_count > 0 ? total_recv_latency / recv_count * 1000.0 : 0.0; // Усреднено по всем приёмам
    double throughput_mb_s = total_data_bytes / (1024.0 * 1024.0) / duration_sec;
    double avg_msg_size_kb = (sent_count + recv_count > 0) ? (double)total_data_bytes / (sent_count + recv_count) : 0; // Усреднено по всем обработанным сообщениям

    // Консоль summary
    printf("\n=== Client Summary ===\n");
    printf("Duration: %.2f s | Messages: %u sent, %u received (%.1f%% overall success)\n", duration_sec, sent_count, recv_count, overall_success);
    printf("Loss: %.1f%% (Out-of-order: %u, Lost: %u)\n", loss_rate, out_of_order_loss, total_lost);
    printf("Speeds: Send %.0f msg/s (%.1f%% success), Recv %.0f msg/s (%.1f%% success)\n", send_rate, send_success, recv_rate, recv_success);
    printf("Latencies: Avg %.1f ms (Median %.1f, Send %.1f, Recv %.1f)\n", avg_latency_ms, median_latency, avg_send_latency_ms, avg_recv_latency_ms);
    printf("Throughput: %.1f MB/s (Peak %.1f) | Avg Msg: %.1f KB | Max Buffer: %.1f%%\n", throughput_mb_s, peak_throughput, avg_msg_size_kb, max_buffer_usage);
    printf("Retries: Avg %.2f, Max %u | Errors: %u (Full: %u)\n", (double)total_retries / (2 * SHM_TEST_NUM_MESSAGES), max_retries, error_count, full_count);
    const char* grade = (overall_success > 99 ? "A (Excellent)" : (overall_success > 95 ? "B (Good)" : (overall_success > 90 ? "C (Fair)" : (overall_success > 80 ? "D (Poor)" : "F (Fail)"))));
    printf("Quality Grade: %s\n", grade);
    if (overall_success < 95) printf("Recommendation: Increase buffer if full >10%%, or reduce NUM_MESSAGES.\n");

    // Append to log
    FILE* log = fopen("test_log.txt", "a"); // Используем имя файла из shm_types.h или константы
    if (log) {
        fprintf(log, "\n--- Client Run %s ---\n", ctime(&now));
        fprintf(log, "Duration: %.2f s | Success: %.1f%% | Loss: %.1f%% | Throughput: %.1f MB/s | Grade: %s\n", duration_sec, overall_success, loss_rate, throughput_mb_s, grade);
        fclose(log);
        printf("Appended to log: test_log.txt\n");
    }

    // Report файл (full)
    FILE* report = fopen(report_file, "w");
    if (report) {
        fprintf(report, "SHM IPC Bi-Dir Stress Test Report (Client) - %s", ctime(&now));
        fprintf(report, "====================================\n");
        fprintf(report, "Role: Client | Duration: %.3f sec\n", duration_sec);
        fprintf(report, "Send Stats: %u msgs (%.1f%% success, %.0f msg/s)\n", sent_count, send_success, send_rate);
        fprintf(report, "Recv Stats: %u msgs (%.1f%% success, %.0f msg/s)\n", recv_count, recv_success, recv_rate);
        fprintf(report, "Loss: %u out-of-order + %u lost (%.2f%% total)\n", out_of_order_loss, total_lost, loss_rate);
        fprintf(report, "Errors: %u total (Full: %u, Other: %u)\n", error_count + full_count, full_count, error_count);
        fprintf(report, "Overall Success: %.2f%% | Grade: %s\n", overall_success, grade);
        fprintf(report, "Latency: Avg %.2f ms (Median %.2f, Send %.2f, Recv %.2f)\n", avg_latency_ms, median_latency, avg_send_latency_ms, avg_recv_latency_ms);
        fprintf(report, "Throughput: %.2f MB/s (Peak %.2f) | Avg Msg Size: %.1f KB\n", throughput_mb_s, peak_throughput, avg_msg_size_kb);
        fprintf(report, "Retries: Avg %.2f, Max %u | Max Buffer Usage: %.1f%%\n", (double)total_retries / (2 * SHM_TEST_NUM_MESSAGES), max_retries, max_buffer_usage);
        if (overall_success < 95) fprintf(report, "Recommendation: Increase buffer if full >10%%, or reduce NUM_MESSAGES.\n");
        fclose(report);
        printf("Full report: %s\n", report_file);
    }

    // CSV (intervals)
    FILE* csv = fopen(csv_file, "w");
    if (csv) {
        fprintf(csv, "Interval,Time_Sec,Sent,Recv,Latency_ms,Throughput_MB_s,Running_Loss_Pct\n");
        fprintf(csv, "Final,%.3f,%u,%u,%.2f,%.2f,%.2f\n", duration_sec, sent_count, recv_count, avg_latency_ms, throughput_mb_s, loss_rate);
        fclose(csv);
        printf("CSV data: %s\n", csv_file);
    }

    shm_client_disconnect(client);
    return 0;
}