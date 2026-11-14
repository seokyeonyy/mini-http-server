#include "session.h"
#include "session_pool.h"
#include <cstdlib>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstring>

// 전역 세션 목록의 실제 정의 (프로젝트 전체에서 여기 한 곳만)
std::vector<ClientSession*> g_sessions;

// 세션 생성(풀에서 빌려옴)
ClientSession* create_session(int fd) {
    ClientSession* s = pool_alloc();
    if (!s) return NULL; // 풀이 바닥남

    // 재사용되는 세션이라 이전 연결의 값이 남아 있음 → 필드를 직접 초기화
    // (버퍼 16KB 전체를 지우지 않고, 유효 범위를 나타내는 필드만 초기화)
    s->fd = fd;
    s->state = STATE_READING_HEADER; // 초기 상태: HTTP 헤더 읽기
    
    s->read_pos=0;
    s->read_buf[0]='\0';
    s->write_pos=0;
    s->write_len=0;

    s->method[0]='\0';
    s->path[0]='\0';
    s->version[0]='\0';
    s->content_length=0;

    s->keep_alive=0;
    s->req_len=0;
    s->req_count=0;
    s->last_active=time(NULL);

    g_sessions.push_back(s);
    return s;

}

// 세션 종료 및 정리
void close_session(int epoll_fd, ClientSession* s)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, s->fd, NULL);
    close(s->fd);

    for (auto it = g_sessions.begin(); it != g_sessions.end(); ++it)
    {
        if (*it == s) {
            g_sessions.erase(it);
            break;
        }
    }

    pool_free(s); // free(s) 대신 풀에 반납
}

//처리한 요청만큼 버퍼에서 제거하고, 남은 바이트(다음 요청)를 앞으로 당김
void reset_session(ClientSession* s)
{
    size_t remain=s->read_pos-s->req_len;
    memmove(s->read_buf, s->read_buf+s->req_len, remain);
    s->read_pos=remain;
    s->read_buf[s->read_pos]='\0';

    s->method[0]=s->path[0]=s->version[0]='\0';
    s->write_pos=s->write_len=0;
    s->req_len=0;
    s->keep_alive=0;
    s->req_count++;
    s->state=STATE_READING_HEADER;
    s->last_active=time(NULL);

}