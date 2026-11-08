#include "session.h"
#include <cstdlib>
#include <unistd.h>
#include <sys/epoll.h>

// 전역 세션 목록의 실제 정의 (프로젝트 전체에서 여기 한 곳만)
std::vector<ClientSession*> g_sessions;

// 세션 생성
ClientSession* create_session(int fd) {
    // 세션 구조체 메모리 동적 할당 (0으로 초기화)
    ClientSession* s = (ClientSession*)calloc(1, sizeof(ClientSession));
    if (!s) return NULL; // 메모리 할당 실패 예외 처리

    s->fd = fd;
    s->state = STATE_READING_HEADER; // 초기 상태: HTTP 헤더 읽기
    s->last_active = time(NULL);     // 현재 시간 기록
    g_sessions.push_back(s);         // 생성된 세션을 벡터에 추가

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

    free(s);
}