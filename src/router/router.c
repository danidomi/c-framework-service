#include "router.h"

#include <stdlib.h>
#include <string.h>

#define MAX_ROUTES 64

typedef struct {
    Method method;
    char *path;
    RouteHandler handler;
} Route;

static Route routes[MAX_ROUTES];
static size_t route_count = 0;

int route_register(Method method, const char *path, RouteHandler handler) {
    if (!path || !handler) return -1;
    if (route_count >= MAX_ROUTES) return -1;
    char *p = strdup(path);
    if (!p) return -1;
    routes[route_count].method = method;
    routes[route_count].path = p;
    routes[route_count].handler = handler;
    route_count++;
    return 0;
}

Response *route_dispatch(Request *request) {
    if (!request || !request->path) return NULL;
    for (size_t i = 0; i < route_count; i++) {
        if (routes[i].method == request->method &&
            strcmp(routes[i].path, request->path) == 0) {
            return routes[i].handler(request);
        }
    }
    return NULL;
}

void route_clear(void) {
    for (size_t i = 0; i < route_count; i++) {
        free(routes[i].path);
        routes[i].path = NULL;
        routes[i].handler = NULL;
    }
    route_count = 0;
}
