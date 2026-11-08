#pragma once
#include "session.h"
 
// read_buf에 완성된 헤더("\r\n\r\n")가 있다고 가정하고,
// 요청을 파싱해서 응답을 write_buf에 만든다.
void handle_request(ClientSession* s);