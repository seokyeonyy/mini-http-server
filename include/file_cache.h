#ifndef FILE_CACHE_H
#define FILE_CACHE_H

#include <sys/types.h>
#include <ctime>
#include <cstddef>

// 캐시에 저장되는 파일 하나의 정보
struct CacheEntry
{
    int fd;  // 열어둔 파일 fd (여러 연결이 공유)
    off_t size;  // 파일 크기 (Content-Length에 사용)
    ino_t ino;   // inode 번호: 파일이 통째로 교체되었는지 감지
    struct timespec mtime;   // 수정 시각 (나노초 단위까지 비교)
    time_t validated; // 마지막으로 stat으로 확인한 시각 (11단계)
    int refcount; // 지금 이 fd로 전송 중인 연결 수 (12단계)
    bool stale; // 캐시에서 빠졌지만 아직 사용 중 (마지막 사용자가 정리)
};

// 경로에 해당하는 파일 정보를 돌려줌.
// 캐시에 있으면 그대로, 없으면 open + fstat 후 캐시에 저장.
// 성공 시 refcount를 1 올림 → 다 쓰면 반드시 cache_release 호출.
// 파일이 없거나 일반 파일이 아니면 NULL
CacheEntry* cache_get(const char* path);

// 전송이 끝났거나 연결이 끊겼을 때 호출 (refcount를 1 내림).
// fd는 닫지 않음: 캐시가 계속 들고 있다가 다음 요청에 재사용.
void cache_release(CacheEntry* e);


// 현재 캐시에 저장된 파일 수 (디버깅, 테스트용)
size_t cache_count();



#endif