#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../logger/logger.h"

struct ThreadArgs {
    int sockfd;
    struct sockaddr_in client_addr;
};

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

static int send_response(int sockfd, Response *r) {
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

static void *handle_connection(void *arg) {
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int sockfd = args->sockfd;
    struct sockaddr_in client = args->client_addr;
    free(args);

    Request *request = request_recv(sockfd);
    if (!request) {
        log_message(WARNING, "Failed to read/parse request from %s:%d",
                    inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        send_404(sockfd);
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return NULL;
    }

    log_message(DEBUG, "%s %s from %s:%d",
                method_name(request->method), request->path,
                inet_ntoa(client.sin_addr), ntohs(client.sin_port));

    Response *response = route_dispatch(request);
    if (!response) {
        send_404(sockfd);
    } else {
        if (response->data_length > 0) {
            int has_cl = 0;
            for (size_t i = 0; i < response->headersCount; i++) {
                if (strncasecmp(response->headers[i], "Content-Length:", 15) == 0) {
                    has_cl = 1;
                    break;
                }
            }
            if (!has_cl) {
                char cl[64];
                snprintf(cl, sizeof(cl), "Content-Length: %zu", response->data_length);
                response_add_header(response, cl);
            }
        }
        if (send_response(sockfd, response) != 0) {
            log_message(WARNING, "Failed to send response");
        }
        response_free(response);
    }

    request_free(request);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
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

    log_message(INFO, "Accepting web requests on port %d", port);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t sin_size = sizeof(client_addr);
        int new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_sockfd == -1) {
            log_message(WARNING, "accept failed: %s", strerror(errno));
            continue;
        }

        struct ThreadArgs *args = malloc(sizeof(*args));
        if (!args) {
            close(new_sockfd);
            log_message(ERROR, "out of memory accepting connection");
            continue;
        }
        args->sockfd = new_sockfd;
        args->client_addr = client_addr;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_connection, args) != 0) {
            close(new_sockfd);
            free(args);
            log_message(ERROR, "pthread_create failed: %s", strerror(errno));
            continue;
        }
        pthread_detach(thread);
    }

    close(sockfd);
    return 0;
}
