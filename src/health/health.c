#include "health.h"

#include "../logger/logger.h"
#include "../metrics/metrics.h"
#include "../router/router.h"
#include "../server/server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static ReadinessCheck readiness_check = NULL;

void health_set_readiness_check(ReadinessCheck check) {
    readiness_check = check;
}

Response *handle_healthz(Request *req) {
    (void)req;
    Response *r = response_new(HTTP_OK);
    if (!r) return NULL;
    response_add_header(r, "Content-Type: application/json; charset=utf-8");
    response_set_body(r, "{\"status\":\"ok\"}");
    return r;
}

Response *handle_readyz(Request *req) {
    (void)req;
    int ready = readiness_check ? readiness_check() : 1;
    Response *r = response_new(ready ? HTTP_OK : HTTP_SERVICE_UNAVAILABLE);
    if (!r) return NULL;
    response_add_header(r, "Content-Type: application/json; charset=utf-8");
    response_set_body(r, ready ? "{\"status\":\"ready\"}" : "{\"status\":\"not_ready\"}");
    return r;
}

Response *handle_metrics(Request *req) {
    (void)req;
    if (!metrics_is_enabled()) {
        Response *r = response_new(HTTP_NOT_FOUND);
        if (!r) return NULL;
        response_add_header(r, "Content-Type: application/json; charset=utf-8");
        response_set_body(r, "{\"error\":\"metrics disabled\"}");
        return r;
    }
    size_t len = 0;
    char *body = metrics_render_prometheus(&len);
    if (!body) return NULL;
    Response *r = response_new(HTTP_OK);
    if (!r) { free(body); return NULL; }
    response_add_header(r, "Content-Type: text/plain; version=0.0.4; charset=utf-8");
    response_set_body_n(r, body, len);
    free(body);
    return r;
}

void health_register_default_routes(void) {
    route_register(GET, "/healthz", handle_healthz);
    route_register(GET, "/readyz",  handle_readyz);
    route_register(GET, "/metrics", handle_metrics);
}

/* ---------------------------------------------------------------------
 * Admin server: a small dedicated HTTP listener for /healthz, /readyz,
 * /metrics. Runs on its own thread so it doesn't compete with the main
 * router. Single-threaded request handling per accept — admin scrapes
 * are infrequent enough that this is fine and keeps complexity low.
 * --------------------------------------------------------------------- */

static Response *admin_dispatch(Request *req) {
    if (!req || !req->path || req->method != GET) return NULL;
    if (strcmp(req->path, "/healthz") == 0) return handle_healthz(req);
    if (strcmp(req->path, "/readyz")  == 0) return handle_readyz(req);
    if (strcmp(req->path, "/metrics") == 0) return handle_metrics(req);
    return NULL;
}

static void admin_handle_conn(int sockfd) {
    ConnState c;
    conn_state_init(&c, sockfd);
    Request *req = request_recv(&c);
    if (!req) {
        close(sockfd);
        return;
    }

    Response *resp = admin_dispatch(req);
    if (!resp) {
        resp = response_new(HTTP_NOT_FOUND);
        if (resp) {
            response_add_header(resp, "Content-Type: application/json; charset=utf-8");
            response_set_body(resp, "{\"error\":\"not found\"}");
        }
    }

    if (resp) {
        if (resp->data_length > 0 && !response_has_header(resp, "Content-Length")) {
            char cl[64];
            snprintf(cl, sizeof(cl), "Content-Length: %zu", resp->data_length);
            response_add_header(resp, cl);
        } else if (resp->data_length == 0 && !response_has_header(resp, "Content-Length")) {
            response_add_header(resp, "Content-Length: 0");
        }
        if (!response_has_header(resp, "Connection")) {
            response_add_header(resp, "Connection: close");
        }
        send_response(sockfd, resp);
        response_free(resp);
    }

    request_free(req);
    close(sockfd);
}

static void *admin_loop(void *arg) {
    int port = (int)(intptr_t)arg;

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        log_message(ERROR, "admin: socket: %s", strerror(errno));
        return NULL;
    }
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_message(ERROR, "admin: bind :%d: %s", port, strerror(errno));
        close(srv);
        return NULL;
    }
    if (listen(srv, 8) < 0) {
        log_message(ERROR, "admin: listen: %s", strerror(errno));
        close(srv);
        return NULL;
    }

    log_message(INFO, "admin endpoints (/healthz, /readyz, /metrics) on port %d", port);

    for (;;) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int sockfd = accept(srv, (struct sockaddr *)&client, &len);
        if (sockfd < 0) {
            if (errno == EINTR) continue;
            log_message(WARNING, "admin: accept: %s", strerror(errno));
            continue;
        }
        admin_handle_conn(sockfd);
    }
}

void health_start_admin_server(int port) {
    if (port <= 0) {
        const char *env = getenv("ADMIN_PORT");
        port = (env && *env) ? atoi(env) : 9090;
    }
    if (port <= 0 || port > 65535) {
        log_message(ERROR, "admin: invalid port %d, not starting", port);
        return;
    }

    pthread_t t;
    if (pthread_create(&t, NULL, admin_loop, (void *)(intptr_t)port) != 0) {
        log_message(ERROR, "admin: pthread_create: %s", strerror(errno));
        return;
    }
    pthread_detach(t);
}
