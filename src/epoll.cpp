#include "server.h"
#include "session.h"
#include "http_parser.h"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

using std::cout;
using std::endl;

#define MAX_EVENTS 1024

// Non-blocking Read Handler
static void handle_read(int epoll_fd, ClientSession* s)
{
    s->last_active = time(NULL);

    while (1) {
        ssize_t bytes = read(s->fd, s->read_buf + s->read_pos,
                             sizeof(s->read_buf) - s->read_pos - 1);

        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { break; } // 더 이상 읽을 데이터 없음

            // real I/O error
            close_session(epoll_fd, s);
            return;
        }
        // client EOF
        else if (bytes == 0)
        {
            close_session(epoll_fd, s);
            return;
        }
        s->read_pos += bytes;
        s->read_buf[s->read_pos] = '\0';

        // 헤더가 완성되면 파싱과 응답 생성은 http_Parsing.cpp에 위임
        if (s->state == STATE_READING_HEADER &&
            strstr(s->read_buf, "\r\n\r\n") != NULL)
        {
            handle_request(s);

            // EPOLLOUT 이벤트 감시로 전환
            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = s;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, s->fd, &ev);
            break;
        }
    }
}

// Non-blocking Write Handler
static void handle_write(int epoll_fd, ClientSession* s)
{
    s->last_active = time(NULL);
    while (s->write_pos < s->write_len)
    {
        ssize_t bytes = write(s->fd, s->write_buf + s->write_pos,
                              s->write_len - s->write_pos);

        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { return; } // 송신 버퍼 가득 참 -> 다음 EPOLLOUT 대기

            // response error
            close_session(epoll_fd, s);
            return;
        }
        s->write_pos += bytes;
    }
    close_session(epoll_fd, s);   // keep-alive 구현 시 이 부분을 바꾸게 됨
}

// Event Loop
void run_event_loop(int epoll_fd, int listen_fd)
{
    signal(SIGPIPE, SIG_IGN);
    struct epoll_event events[MAX_EVENTS];

    while (1)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000); // 1s timeout



        for (int i = 0; i < nfds; i++)
        {
            // Listen socket: 새 연결
            if (events[i].data.fd == listen_fd) {
                // EAGAIN이 뜰 때까지 반복하여 대기 중인 모든 접속 요청 처리
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);

                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 정상 소진
                        if (errno == EINTR) continue;                       // 시그널로 중단
                        break;
                    }

                    // Non-blocking 전환
                    fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);

                    // 세션 생성 및 epoll 등록
                    ClientSession* s = create_session(client_fd);
                    if (s) {
                        struct epoll_event ev;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.ptr = s;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                    }
                }
            }
            // 클라이언트 입출력 처리
            else {
                ClientSession* s = (ClientSession*)events[i].data.ptr;

                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    close_session(epoll_fd, s);
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    handle_read(epoll_fd, s);
                } else if (events[i].events & EPOLLOUT) {
                    handle_write(epoll_fd, s);
                }
            }
        }
        // [타임아웃 체크 로직]
        time_t current_time = time(NULL);
        const double TIMEOUT_LIMIT = 10.0;

        for (auto it = g_sessions.begin(); it != g_sessions.end();)
        {
            ClientSession* s = *it;
            if (current_time - s->last_active > TIMEOUT_LIMIT)
            {
                cout << "Session timeout! FD: " << s->fd << endl;
                close_session(epoll_fd, s);   // 이 안에서 g_sessions.erase 호출
                it = g_sessions.begin();
            } else {
                ++it;
            }
        }
    }
}