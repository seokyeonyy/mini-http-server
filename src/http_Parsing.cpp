#include "http_parser.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
//[1] 상태 코드와 메시지만 주면 간단한 HTML 응답을 만들어 주는 함수
static void build_simple_response(ClientSession* s, int code, const char* msg)
{
    char body[256];
    snprintf(body, sizeof(body), "<html><body><h1>%d%s</h1></body></html>", code, msg);
    s->write_len=snprintf(s->write_buf, sizeof(s->write_buf),
      "HTTP/1.1 %d %s\r\n"
      "Content-Length: %zu\r\n"
      "Content-Type: text/html\r\n"
      "Connection: close\r\n\r\n%s",
      code, msg, strlen(body), body);

    s->write_pos=0;
    s->state = STATE_WRITING;

}

// 요청 파싱 + 응답 생성 (epoll은 전혀 모름)
void handle_request(ClientSession* s)
{
    //[2]폭 제한 + HTTP 버전까지 파싱, 형식이 틀리면 400
    if(sscanf(s->read_buf, "%15s %225s %15s", s->method, s->path, s->version)!=3)
    {
        build_simple_response(s, 400, "Bad Request");
        return;
    }
    //[3]경로 탐색 차단: '/'로 시작하지 않거나 ".."이 있으면 403
    if(s->path[0]!='/' || strstr(s->path, "..")!=NULL)
    {
        build_simple_response(s, 403, "Forbidden");
        return;
    }
    

    char file_path[512];
    if (strcmp(s->path, "/") == 0)
    {
        snprintf(file_path, sizeof(file_path), "./www/index.html");
    } else {
        snprintf(file_path, sizeof(file_path), "./www%s", s->path);
    }

    int fd = open(file_path, O_RDONLY);
    if (fd < 0)
    {
        const char* body = "<html><body><h1>404 Not Found</h1></body></html>";
        snprintf(s->write_buf, sizeof(s->write_buf),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Length: %zu\r\n"
                 "Content-Type: text/html\r\n"
                 "Connection: close\r\n\r\n%s",
                 strlen(body), body);
        s->write_len = strlen(s->write_buf);   // [수정] 원본에 빠져 있던 줄
    }
    else {
        off_t fsize = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        int header_len = snprintf(s->write_buf, sizeof(s->write_buf),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Length: %ld\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Connection: close\r\n\r\n",
                                  (long)fsize);

        
        //[4] 임시 방어: 본문이 버퍼에 다 안 들어가면 500 (sendfile 적용 전까지)
        if((size_t)fsize>sizeof(s->write_buf)-header_len-1)
        {
            close(fd);
            build_simple_response(s, 500, "Imtermal Server Error");
            return;
        }
        
        read(fd, s->write_buf + header_len, sizeof(s->write_buf) - header_len - 1);
        s->write_len = header_len + fsize;
        close(fd);

        s->write_pos=0;
        s->state=STATE_WRITING;

     
         
    }

}