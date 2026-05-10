#pragma once

#include "../request/request.h"
#include "../response/response.h"
#include "../router/router.h"

#ifndef PORT
#define PORT 8080
#endif

int server_run(int port);
void fatal(const char *msg);

/* Building blocks exposed so secondary servers (e.g. an admin/metrics
 * port) can reuse the framework's response-sending logic. */
int send_response(int sockfd, Response *r);
int response_has_header(Response *r, const char *name);
