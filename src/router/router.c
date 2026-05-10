#include "router.h"
#include "../request/request.h"

#include <stdlib.h>
#include <string.h>

#define MAX_ROUTES 64
#define MAX_PATH_PARAM_KEY 64
#define MAX_PATH_PARAM_VAL 256

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

/* Match `pattern` (possibly containing :name segments) against
 * request->path. On match, each :name segment is bound as a query
 * param on the request via request_add_query_param() so handlers
 * can read it via the existing get_query_param_value() API.
 * On no-match, partial bindings are rolled back. */
static int match_and_bind(const char *pattern, Request *request) {
    size_t saved_count = request->queryParamsCount;
    const char *p = pattern;
    const char *u = request->path;

    while (*p && *u) {
        const char *pseg = p, *useg = u;
        while (*pseg && *pseg != '/') pseg++;
        while (*useg && *useg != '/') useg++;
        size_t plen = (size_t)(pseg - p), ulen = (size_t)(useg - u);

        if (plen > 1 && p[0] == ':') {
            if (ulen == 0) goto fail;
            if (plen - 1 >= MAX_PATH_PARAM_KEY) goto fail;
            if (ulen >= MAX_PATH_PARAM_VAL) goto fail;
            char key[MAX_PATH_PARAM_KEY];
            char value[MAX_PATH_PARAM_VAL];
            memcpy(key,   p + 1, plen - 1); key[plen - 1] = '\0';
            memcpy(value, u,     ulen);     value[ulen]   = '\0';
            if (request_add_query_param(request, key, value) != 0) goto fail;
        } else {
            if (plen != ulen || strncmp(p, u, plen) != 0) goto fail;
        }

        p = pseg;
        u = useg;
        if (*p == '/' && *u == '/') { p++; u++; }
        else if (*p != *u) goto fail;
    }

    if (*p != '\0' || *u != '\0') goto fail;
    return 1;

fail:
    while (request->queryParamsCount > saved_count) {
        request->queryParamsCount--;
        free(request->queryParams[request->queryParamsCount].key);
        free(request->queryParams[request->queryParamsCount].value);
        request->queryParams[request->queryParamsCount].key = NULL;
        request->queryParams[request->queryParamsCount].value = NULL;
    }
    return 0;
}

Response *route_dispatch(Request *request) {
    if (!request || !request->path) return NULL;
    for (size_t i = 0; i < route_count; i++) {
        if (routes[i].method != request->method) continue;
        if (match_and_bind(routes[i].path, request)) {
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
