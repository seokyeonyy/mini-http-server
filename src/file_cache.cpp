#include "file_cache.h"

#include <string>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define CACHE_VALID_SEC 5 
// 경로 문자열 → 파일 정보
static std::unordered_map<std::string, CacheEntry*> g_cache;

// 항목을 완전히 정리 (fd 닫기 + 메모리 해제)
static void entry_destroy(CacheEntry* e)
{
    close(e->fd);
    delete e;
}
// 캐시에서 항목을 뺌. 아무도 안 쓰면 바로 정리하고,
// 전송 중인 연결이 있으면 stale 표시만 해두고 마지막 사용자에게 맡김
static void invalidate(std::unordered_map<std::string, CacheEntry*>::iterator it)
{
    CacheEntry* e=it->second;
    g_cache.erase(it);

    if(e->refcount==0){entry_destroy(e);}
    else{e->stale=true;}
    
}
// 캐시된 정보와 지금 디스크의 파일이 같은지 비교
static bool same_file(const CacheEntry* e, const struct stat& st)
{
    return e->ino==st.st_ino && e->size==st.st_size&& e->mtime.tv_sec==st.st_mtim.tv_sec&&e->mtime.tv_nsec==st.st_mtim.tv_nsec;
}

CacheEntry* cache_get(const char* path)
{
    time_t now=time(NULL);

    auto it = g_cache.find(path);
    if(it!=g_cache.end())
    {
        CacheEntry* e=it->second;

        // 1. 최근에 확인한 항목: 시스템 콜 없이 바로 사용
        if(now-e->validated<CACHE_VALID_SEC)
        {
            e->refcount++;
            return e;
        }
        // 2. 유효 시간이 지남: stat 한 번으로 바뀌었는지 확인
        struct stat st;
        if(stat(path, &st)==0 && S_ISREG(st.st_mode)&&same_file(e, st))
        {
            e->validated=now;
            e->refcount++;
            return e;
        }
        // 3. 바뀌었거나 삭제됨: 캐시에서 빼고 아래에서 새로 열기
        invalidate(it);

    }
    // 캐시 미스: 실제로 열어서 정보 확인
    int fd=open(path, O_RDONLY);
    if(fd<0) return NULL;

    struct stat st;
    if(fstat(fd, &st)<0 || !S_ISREG(st.st_mode))
    {
        return NULL;
    }
    CacheEntry* e=new CacheEntry;
    e->fd=fd;
    e->size=st.st_size;
    e->ino=st.st_ino;
    e->mtime=st.st_mtim;
    e->validated=now;
    e->refcount=1;
    e->stale=false;

    g_cache[path]=e;
    return e;


}

void cache_release(CacheEntry* e)
{
    if(!e) return;
    if (e->refcount > 0) {
        e->refcount--;
    }
    if(e->stale&&e->refcount==0)
    {
        entry_destroy(e);
    }
}

size_t cache_count()
{
    return g_cache.size();
}