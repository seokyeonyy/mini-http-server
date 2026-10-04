#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>

using std::cout;
using std::endl;


//1.
typedef enum{
    STATE_READING_HEADER,
    STATE_READING_BODY,
    STATE_PROCESSING,
    STATE_WRITING,
    STATE_DISCONNECTING
}ConnState;

//2.ClientSession struct 
typedef struct {
    int fd;
    ConnState state;

    char read_buf[8192];
    size_t read_pos;

    char write_buf[8192];
    size_t write_pos;
    size_t write_len;

    char method[16];
    char path[256];
    size_t content_length;

    time_t last_active;
} ClientSession;

// 1.Create_Session  정의 
ClientSession* create_session(int fd) {
    // 세션 구조체 메모리 동적 할당 (0으로 초기화)
    ClientSession* s = (ClientSession*)calloc(1, sizeof(ClientSession));
    if (!s) return NULL; // 메모리 할당 실패 예외 처리

    s->fd = fd;
    s->state = STATE_READING_HEADER; // 초기 상태: HTTP 헤더 읽기
    s->last_active = time(NULL);     // 현재 시간 기록

    return s;
}

#define MAX_EVENTS 1024

//세션 종료 및 정리
void close_session(int epoll_fd, ClientSession* s)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, s->fd, NULL);
    close(s->fd);
    free(s);
}
//Non-blocking Read & HTTP Parshig Handler
void handle_read(int epoll_fd, ClientSession* s)
{
    s->last_active=time(NULL);

    while(1){
        ssize_t bytes=read(s->fd, s->read_buf+s->read_pos, sizeof(s->read_buf+ s->write_pos, s->write_len - s->write_pos));
    
        if(bytes<0)
        {
            if(errno==EAGAIN||errno==EWOULDBLOCK){break;}  // 더 이상 읽을 데이터가 없음 (정상 소진)

            //real I/O error
            close_session(epoll_fd, s);
            return;
 
        }
        //client EOF
        else if(bytes==0)
        {
            close_session(epoll_fd, s);
            return;
        }
        s->read_pos+=bytes;
        s->read_buf[s->read_pos]='\0';

        //HTTP Parshing
        if(s->state==STATE_READING_HEADER)
        {
            char* header_end=strstr(s->read_buf, "\r\n\r\n");
            if(header_end != NULL)
            {
                //response buffer
                const char* body="<html><body><h1>200 OK</h1></body></html>";
                snprintf(s->write_buf, sizeof(s->write_buf),
                "HTTP/1.1 200 OK\r\n"
                "Content-Length:%zu\r\n"
                "Content-Type:text/html\r\n"
                "Connection:close\r\n\r\n%s",
                strlen(body), body);

                s->write_len=strlen(s->write_buf);
                s->write_pos=0;
                s->state=STATE_WRITING;

                //// EPOLLOUT 이벤트 감시로 전환
                struct epoll_event ev;
                ev.events=EPOLLOUT|EPOLLET;
                ev.data.ptr=s;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, s->fd, &ev);
                break;
            }
        }
    
    }
}

//Non-blocking Write Handler
void handle_write(int epoll_fd, ClientSession* s)
{
    s->last_active=time(NULL);
    while(s->write_pos < s->write_len)
    {
        ssize_t bytes=write(s->fd, s->write_buf + s->write_pos, s->write_len - s->write_pos);
    
        if(bytes<0)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK){return;} // 커널 소켓 송신 버퍼가 가득 찬 상태 -> 다음 EPOLLOUT 대기

            //response error
            close_session(epoll_fd, s);
            return;

        }
        s->write_pos+=bytes;
      
    }
    close_session(epoll_fd, s);
}

//Event Loop
void run_event_loop(int epoll_fd, int listen_fd)
{
    signal(SIGPIPE, SIG_IGN);
    struct epoll_event events[MAX_EVENTS];

    while(1)
    {
        int nfds=epoll_wait(epoll_fd, events, MAX_EVENTS, 1000); //1s timeout
        for(int i=0;i<nfds;i++)
        {
            //Listen socket new connection
            if(events[i].data.fd==listen_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_len=sizeof(client_addr);
                int client_fd=accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            
                if(client_fd != -1)
                {
                    fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0)| O_NONBLOCK);

                
                    ClientSession* s= create_session(client_fd);
                    struct epoll_event ev;
                    ev.events=EPOLLIN|EPOLLET;
                    ev.data.ptr=s;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                
                
                }
            
            }
            // 클라이언트 데이터 입출력 처리
            else{
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
 }
}


int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080); // 8080 포트 사용

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        perror("listen failed");
        return 1;
    }

    // listen 소켓 Non-blocking 설정
    fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFL, 0) | O_NONBLOCK);

    // epoll 인스턴스 생성 및 listen_fd 등록 
    int epoll_fd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    std::cout << "Server listening on port 8080..." << std::endl;

    // 이벤트 루프 실행
    run_event_loop(epoll_fd, listen_fd);

    close(listen_fd);
    close(epoll_fd);
    return 0;
}