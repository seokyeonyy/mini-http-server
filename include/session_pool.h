#ifndef SESSION_POOL_H
#define SESSION_POOL_H

#include "session.h"

void pool_init();
ClientSession* pool_alloc();
void pool_free(ClientSession* s);
int pool_available();




#endif