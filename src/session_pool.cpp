#include "session_pool.h"
#include <cstdio>
#include <sanitizer/asan_interface.h> 
#define POOL_SIZE 1024

// 세션 본체: 서버 시작 시점에 1024개를 한 번에 확보 (약 16MB)
static ClientSession g_pool[POOL_SIZE];

// 빈 세션을 가리키는 포인터들을 쌓아두는 스택
static ClientSession* g_free_list[POOL_SIZE];
static int g_free_top=0;  // 스택에 쌓인 개수 = 남은 빈 세션 수

void pool_init()
{
    for(int i=0;i<POOL_SIZE;i++)
    {
        g_free_list[i]=&g_pool[i];
        ASAN_POISON_MEMORY_REGION(&g_pool[i], sizeof(ClientSession));    // 처음엔 전부 사용 금지
    }
    g_free_top=POOL_SIZE;
}
ClientSession* pool_alloc()
{
    if(g_free_top==0) return NULL;   // 풀이 바닥남
    

    ClientSession* s=g_free_list[--g_free_top];
    ASAN_UNPOISON_MEMORY_REGION(s, sizeof(ClientSession));
    return s;
}

void pool_free(ClientSession* s)
{
    // 풀 소속이 아닌 포인터가 들어오면 반납하지 않음 (실수 방지)
    if(s<&g_pool[0] || s>&g_pool[POOL_SIZE-1])
    {
        fprintf(stderr, "pool_free:invalid pointer %p\n", (void*)s);
        return;
    }
    // 스택이 이미 가득 찼다면 같은 세션을 두 번 반납한 것
    if(g_free_top>=POOL_SIZE)
    {
        fprintf(stderr, "pool_free:double free detached\n");
        return;
    }
    ASAN_POISON_MEMORY_REGION(s, sizeof(ClientSession));
    g_free_list[g_free_top++]=s;   // 스택 맨 위에 다시 올려둠
}

int pool_available()
{
    return g_free_top;
}