#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "../logger/logger.h"
#include "../metrics/metrics.h"

#define MAX_CONNECTIONS 256
#define SOCKET_TIMEOUT_SECS 30

struct ThreadArgs {
    int sockfd;
    struct sockaddr_in client_addr;
};

static volatile sig_atomic_t shutting_down = 0;
static int listen_fd_global = -1;

static pthread_mutex_t conn_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t conn_cv = PTHREAD_COND_INITIALIZER;
static int conn_count = 0;

static void on_signal(int sig) {
    (void)sig;
    shutting_down = 1;
    if (listen_fd_global >= 0) {
        shutdown(listen_fd_global, SHUT_RDWR);
    }
}

static void conn_slot_release(void) {
    pthread_mutex_lock(&conn_mu);
    if (conn_count > 0) conn_count--;
    pthread_cond_broadcast(&conn_cv);
    pthread_mutex_unlock(&conn_mu);
}

static int send_all(int sockfd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(sockfd, p, len, 0);
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

int send_response(int sockfd, Response *r) {
    size_t status_len = strlen(r->status_code);
    size_t total = 9 + status_len + 2;
    for (size_t i = 0; i < r->headersCount; i++) {
        total += strlen(r->headers[i]) + 2;
    }
    total += 2;
    total += r->data_length;

    char *buf = malloc(total + 1);
    if (!buf) return -1;

    char *p = buf;
    int n = sprintf(p, "HTTP/1.1 %s\r\n", r->status_code);
    p += n;

    for (size_t i = 0; i < r->headersCount; i++) {
        n = sprintf(p, "%s\r\n", r->headers[i]);
        p += n;
    }

    *p++ = '\r';
    *p++ = '\n';

    if (r->data && r->data_length > 0) {
        memcpy(p, r->data, r->data_length);
        p += r->data_length;
    }

    int rc = send_all(sockfd, buf, (size_t)(p - buf));
    free(buf);
    return rc;
}

static void send_404(int sockfd) {
    static const char body[] =
        "<html><head><title>404 Not Found</title></head>"
        "<body><h1>404 Not Found</h1></body></html>";
    char head[256];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 404 Not Found\r\n"
                     "Content-Type: text/html; charset=utf-8\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n\r\n",
                     sizeof(body) - 1);
    send_all(sockfd, head, (size_t)n);
    send_all(sockfd, body, sizeof(body) - 1);
}

int response_has_header(Response *r, const char *name) {
    size_t name_len = strlen(name);
    for (size_t i = 0; i < r->headersCount; i++) {
        if (strncasecmp(r->headers[i], name, name_len) == 0 &&
            r->headers[i][name_len] == ':') {
            return 1;
        }
    }
    return 0;
}

static void *handle_connection(void *arg) {
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int sockfd = args->sockfd;
    struct sockaddr_in client = args->client_addr;
    free(args);

    struct timeval tv;
    tv.tv_sec = SOCKET_TIMEOUT_SECS;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    ConnState cs;
    conn_state_init(&cs, sockfd);

    int close_conn = 0;
    while (!shutting_down && !close_conn) {
        Request *request = request_recv(&cs);
        if (!request) break;

        log_message(DEBUG, "%s %s from %s:%d",
                    method_name(request->method), request->path,
                    inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        close_conn = (request->http_minor == 0);
        const char *conn_hdr = get_header_value(request, "Connection");
        if (conn_hdr) {
            if (strcasecmp(conn_hdr, "close") == 0) close_conn = 1;
            else if (strcasecmp(conn_hdr, "keep-alive") == 0) close_conn = 0;
        }
        if (shutting_down) close_conn = 1;

        Response *response = route_dispatch(request);
        int status_code;
        if (!response) {
            send_404(sockfd);
            status_code = 404;
            close_conn = 1;
        } else {
            status_code = atoi(response->status_code);
            if (response->data_length > 0 && !response_has_header(response, "Content-Length")) {
                char cl[64];
                snprintf(cl, sizeof(cl), "Content-Length: %zu", response->data_length);
                response_add_header(response, cl);
            } else if (response->data_length == 0 && !response_has_header(response, "Content-Length")) {
                response_add_header(response, "Content-Length: 0");
            }
            if (!response_has_header(response, "Connection")) {
                response_add_header(response, close_conn ? "Connection: close" : "Connection: keep-alive");
            }
            if (send_response(sockfd, response) != 0) {
                log_message(WARNING, "Failed to send response");
                close_conn = 1;
            }
            response_free(response);
        }
        metrics_record_request(status_code);

        request_free(request);
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    conn_slot_release();
    return NULL;
}

void fatal(const char *msg) {
    log_message(ERROR, "fatal: %s (errno=%d: %s)", msg, errno, strerror(errno));
    exit(1);
}

int server_run(int port) {
    int sockfd;
    int yes = 1;
    struct sockaddr_in host_addr;

    metrics_init();

    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        fatal("socket");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        fatal("setsockopt SO_REUSEADDR");
    }

    memset(&host_addr, 0, sizeof(host_addr));
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons((uint16_t)port);
    host_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&host_addr, sizeof(host_addr)) == -1) {
        fatal("bind");
    }
    if (listen(sockfd, 64) == -1) {
        fatal("listen");
    }

    listen_fd_global = sockfd;
    log_message(INFO, "Accepting web requests on port %d (max conns: %d)", port, MAX_CONNECTIONS);

    while (!shutting_down) {
        struct sockaddr_in client_addr;
        socklen_t sin_size = sizeof(client_addr);
        int new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_sockfd == -1) {
            if (shutting_down) break;
            if (errno == EINTR) continue;
            log_message(WARNING, "accept failed: %s", strerror(errno));
            continue;
        }

        pthread_mutex_lock(&conn_mu);
        while (conn_count >= MAX_CONNECTIONS && !shutting_down) {
            pthread_cond_wait(&conn_cv, &conn_mu);
        }
        if (shutting_down) {
            pthread_mutex_unlock(&conn_mu);
            close(new_sockfd);
            break;
        }
        conn_count++;
        pthread_mutex_unlock(&conn_mu);

        struct ThreadArgs *args = malloc(sizeof(*args));
        if (!args) {
            close(new_sockfd);
            conn_slot_release();
            log_message(ERROR, "out of memory accepting connection");
            continue;
        }
        args->sockfd = new_sockfd;
        args->client_addr = client_addr;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_connection, args) != 0) {
            close(new_sockfd);
            free(args);
            conn_slot_release();
            log_message(ERROR, "pthread_create failed: %s", strerror(errno));
            continue;
        }
        pthread_detach(thread);
    }

    log_message(INFO, "shutdown: closing listener, draining connections");
    close(sockfd);
    listen_fd_global = -1;

    pthread_mutex_lock(&conn_mu);
    while (conn_count > 0) {
        pthread_cond_wait(&conn_cv, &conn_mu);
    }
    pthread_mutex_unlock(&conn_mu);

    log_message(INFO, "shutdown complete");
    return 0;
}
