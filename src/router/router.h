#pragma once

#include "../request/request.h"
#include "../response/response.h"

typedef Response *(*RouteHandler)(Request *request);

int route_register(Method method, const char *path, RouteHandler handler);
Response *route_dispatch(Request *request);
void route_clear(void);
