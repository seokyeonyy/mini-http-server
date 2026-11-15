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
#include <sys/sendfile.h>

using std::cout;
using std::endl;

#define MAX_EVENTS 1024

//read_buf에 완성된 요청이 있으면 처리하고 쓰기 모드로 전환
static void try_process(int epoll_fd, ClientSession* s)
{
    if(s->state!=STATE_READING_HEADER) return;
    if(strstr(s->read_buf, "\r\n\r\n")==NULL) return; // 아직 헤더 미완성

    handle_request(s);

    struct epoll_event ev;
    ev.events=EPOLLOUT|EPOLLET|EPOLLRDHUP;
    ev.data.ptr=s;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, s->fd, &ev);

}


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

        try_process(epoll_fd, s);
        if(s->state==STATE_WRITING) break;
    }
}

// Non-blocking Write Handler
static void handle_write(int epoll_fd, ClientSession* s)
{
    
    s->last_active = time(NULL);

    // [1단계] 헤더(또는 짧은 에러 응답 전체) 전송
    while (s->write_pos < s->write_len)
    {
        int flags=(s->file_remain>0)?MSG_MORE:0;
        ssize_t bytes = send(s->fd, s->write_buf + s->write_pos,
                              s->write_len - s->write_pos, flags);

        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { return; } // 송신 버퍼 가득 참 -> 다음 EPOLLOUT 대기

            // response error
            close_session(epoll_fd, s);
            return;
        }
        s->write_pos += bytes;
    }
    // [2단계] 파일 본문을 sendfile로 전송
    while(s->file_remain>0)
    {
        ssize_t sent=sendfile(s->fd, s->file_fd, &s->file_off, s->file_remain);
        if(sent<0)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK) return;
            if(errno==EINTR) continue;
            close_session(epoll_fd, s);
            return;
        }
        if(sent==0)
        {
            // 전송 도중 파일이 잘린 경우: Content-Length를 지킬 수 없으니 연결 종료
            close_session(epoll_fd, s);
            return;
            
        }
        s->file_remain -= sent;
    }

    //[전송 완료] keep-alive
    if(!s->keep_alive)
    {
        close_session(epoll_fd, s);
        return;
    }
    //keep-alive:연결은 유지하고 다음 요청을 기다림
    reset_session(s);

    struct epoll_event ev;
    ev.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    ev.data.ptr=s;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, s->fd, &ev);

    try_process(epoll_fd, s); // 이미 받아둔 다음 요청이 있으면 바로 처리
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

                    ClientSession* s = create_session(client_fd);
                    if (!s) {
                        // 풀이 가득 참: 받을 수 없는 연결은 바로 닫음
                        close(client_fd);
                        continue;
                    }

                    struct epoll_event ev;
                    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    ev.data.ptr = s;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                        // 등록 실패 시에도 세션과 소켓을 정리
                        close_session(epoll_fd, s);
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