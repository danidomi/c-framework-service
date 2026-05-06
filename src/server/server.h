#pragma once

#include "../request/request.h"
#include "../response/response.h"
#include "../router/router.h"

#ifndef PORT
#define PORT 8080
#endif

int server_run(int port);
void fatal(const char *msg);
