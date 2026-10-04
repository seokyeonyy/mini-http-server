#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

const string SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 8080;
const int TOTAL_REQUESTS = 10000; // 총 요청 수
const int CONCURRENCY = 50;        // 동시 스레드 수 (동시 접속 수)

std::atomic<int> g_successful_requests(0);
std::atomic<int> g_failed_requests(0);
std::atomic<long long> g_total_latency_us(0);

void worker_thread(int requests_per_thread) {
    const char* http_request = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    size_t req_len = strlen(http_request);

    for (int i = 0; i < requests_per_thread; ++i) {
        auto start = chrono::high_resolution_clock::now();

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            g_failed_requests++;
            continue;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP.c_str(), &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            g_failed_requests++;
            close(sock);
            continue;
        }

        if (send(sock, http_request, req_len, 0) < 0) {
            g_failed_requests++;
            close(sock);
            continue;
        }

        char buffer[1024];
        ssize_t bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);

        auto end = chrono::high_resolution_clock::now();
        long long latency = chrono::duration_cast<chrono::microseconds>(end - start).count();

        if (bytes_read > 0 && strstr(buffer, "200 OK") != NULL) {
            g_successful_requests++;
            g_total_latency_us += latency;
        } else {
            g_failed_requests++;
        }

        close(sock);
    }
}

int main() {
    cout << "==========================================" << endl;
    cout << "  HTTP Server Load & Concurrency Tester   " << endl;
    cout << "==========================================" << endl;
    cout << "Target: " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "Total Requests: " << TOTAL_REQUESTS << endl;
    cout << "Concurrency (Threads): " << CONCURRENCY << endl;
    cout << "------------------------------------------" << endl;

    int req_per_thread = TOTAL_REQUESTS / CONCURRENCY;
    vector<thread> threads;

    auto global_start = chrono::high_resolution_clock::now();

    for (int i = 0; i < CONCURRENCY; ++i) {
        threads.emplace_back(worker_thread, req_per_thread);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    auto global_end = chrono::high_resolution_clock::now();
    double total_time_sec = chrono::duration<double>(global_end - global_start).count();

    int success = g_successful_requests.load();
    int failed = g_failed_requests.load();
    double rps = (total_time_sec > 0) ? (success / total_time_sec) : 0;
    double avg_latency_ms = (success > 0) ? (g_total_latency_us.load() / (double)success) / 1000.0 : 0.0;

    cout << "\n[ Results ]" << endl;
    cout << "Total Time Taken  : " << total_time_sec << " seconds" << endl;
    cout << "Successful Req    : " << success << endl;
    cout << "Failed Req        : " << failed << endl;
    cout << "Requests Per Sec  : " << rps << " RPS" << endl;
    cout << "Avg Latency       : " << avg_latency_ms << " ms" << endl;
    cout << "==========================================" << endl;

    return 0;
}