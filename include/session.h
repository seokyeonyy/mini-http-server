#pragma once
#include <ctime>
#include <cstddef>
#include <vector>

enum ConnState {
    STATE_READING_HEADER,
    STATE_READING_BODY,
    STATE_PROCESSING,
    STATE_WRITING,
    STATE_DISCONNECTING
};

struct ClientSession {
    int fd;
    ConnState state;

    char read_buf[8192];
    size_t read_pos;

    char write_buf[8192];
    size_t write_pos;
    size_t write_len;

    char method[16];
    char path[256];
    char version[16]; // HTTP/1.0, HTTP/1.1 (keep-alive 판단용)
    size_t content_length;

    int keep_alive;  // 이번 요청 후 연결을 유지할지
    size_t req_len;  // 이번 요청이 read_buf에서 차지한 바이트 수
    int req_count;  // 이 연결에서 처리한 요청 수
    time_t last_active;
};

// 전역 세션 목록: 선언만 하고, 정의는 session.cpp에 있음
extern std::vector<ClientSession*> g_sessions;

ClientSession* create_session(int fd);
void close_session(int epoll_fd, ClientSession* s);
void reset_session(ClientSession* s);