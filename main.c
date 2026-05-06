#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/response/response.h"
#include "src/request/request.h"
#include "src/router/router.h"
#include "src/server/server.h"

static Response *hello_handler(Request *req) {
    Response *r = response_new("200 OK");
    if (!r) return NULL;
    response_add_header(r, "Content-Type: application/json; charset=utf-8");

    const char *name = get_query_param_value(req, "name");
    char body[256];
    snprintf(body, sizeof(body), "{\"message\":\"hello, %s\"}", name ? name : "world");
    response_set_body(r, body);
    return r;
}

static Response *echo_handler(Request *req) {
    Response *r = response_new("200 OK");
    if (!r) return NULL;
    response_add_header(r, "Content-Type: text/plain; charset=utf-8");
    response_set_body_n(r, req->body ? req->body : "", req->bodyLength);
    return r;
}

static Response *health_handler(Request *req) {
    (void)req;
    Response *r = response_new("200 OK");
    if (!r) return NULL;
    response_add_header(r, "Content-Type: application/json");
    response_set_body(r, "{\"status\":\"ok\"}");
    return r;
}

int main(void) {
    route_register(GET,  "/hello",  hello_handler);
    route_register(POST, "/echo",   echo_handler);
    route_register(GET,  "/health", health_handler);

    return server_run(PORT);
}
