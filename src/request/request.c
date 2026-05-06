#include "request.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

#define MAX_REQUEST_HEAD 8192
#define MAX_BODY_SIZE (8 * 1024 * 1024)
#define MAX_QUERY_PARAMS 64
#define MAX_HEADERS_IN 64

static Method method_from_string(const char *s) {
    if (strcmp(s, "GET") == 0) return GET;
    if (strcmp(s, "POST") == 0) return POST;
    if (strcmp(s, "HEAD") == 0) return HEAD;
    if (strcmp(s, "PATCH") == 0) return PATCH;
    if (strcmp(s, "PUT") == 0) return PUT;
    if (strcmp(s, "OPTIONS") == 0) return OPTIONS;
    if (strcmp(s, "DELETE") == 0) return DELETE;
    return METHOD_UNKNOWN;
}

const char *method_name(Method m) {
    switch (m) {
        case GET: return "GET";
        case POST: return "POST";
        case HEAD: return "HEAD";
        case PATCH: return "PATCH";
        case PUT: return "PUT";
        case OPTIONS: return "OPTIONS";
        case DELETE: return "DELETE";
        default: return "UNKNOWN";
    }
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = '\0';
    }
    return s;
}

static int parse_query_string(Request *request, char *queryStart) {
    request->queryParams = calloc(MAX_QUERY_PARAMS, sizeof(KeyValuePair));
    if (!request->queryParams) return -1;

    char *saveptr = NULL;
    char *tok = strtok_r(queryStart, "&", &saveptr);
    while (tok && request->queryParamsCount < MAX_QUERY_PARAMS) {
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            request->queryParams[request->queryParamsCount].key = strdup(tok);
            request->queryParams[request->queryParamsCount].value = strdup(eq + 1);
        } else {
            request->queryParams[request->queryParamsCount].key = strdup(tok);
            request->queryParams[request->queryParamsCount].value = strdup("");
        }
        request->queryParamsCount++;
        tok = strtok_r(NULL, "&", &saveptr);
    }
    return 0;
}

Request *parse_request(const char *raw) {
    if (!raw) return NULL;

    Request *request = calloc(1, sizeof(Request));
    if (!request) return NULL;
    request->method = METHOD_UNKNOWN;

    char *copy = strdup(raw);
    if (!copy) {
        free(request);
        return NULL;
    }

    char *line_save = NULL;
    char *line = strtok_r(copy, "\r\n", &line_save);
    if (!line) goto fail;

    char *tok_save = NULL;
    char *method_str = strtok_r(line, " ", &tok_save);
    char *path_str = strtok_r(NULL, " ", &tok_save);
    if (!method_str || !path_str) goto fail;

    request->method = method_from_string(method_str);
    if (request->method == METHOD_UNKNOWN) goto fail;

    char *query = strchr(path_str, '?');
    if (query) {
        *query = '\0';
        query++;
    }

    request->path = strdup(path_str);
    if (!request->path) goto fail;

    if (query && *query) {
        if (parse_query_string(request, query) != 0) goto fail;
    }

    request->headers = calloc(MAX_HEADERS_IN, sizeof(KeyValuePair));
    if (!request->headers) goto fail;

    while ((line = strtok_r(NULL, "\r\n", &line_save)) != NULL) {
        if (request->headersCount >= MAX_HEADERS_IN) break;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *value = trim(colon + 1);
        char *key = trim(line);
        request->headers[request->headersCount].key = strdup(key);
        request->headers[request->headersCount].value = strdup(value);
        request->headersCount++;
    }

    free(copy);
    return request;

fail:
    free(copy);
    request_free(request);
    return NULL;
}

static ssize_t recv_until_double_crlf(int sockfd, char *buf, size_t cap) {
    size_t total = 0;
    while (total < cap - 1) {
        ssize_t n = recv(sockfd, buf + total, cap - 1 - total, 0);
        if (n <= 0) return n < 0 ? -1 : (ssize_t)total;
        total += (size_t)n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) return (ssize_t)total;
    }
    return -1;
}

Request *request_recv(int sockfd) {
    char *headbuf = malloc(MAX_REQUEST_HEAD);
    if (!headbuf) return NULL;

    ssize_t total = recv_until_double_crlf(sockfd, headbuf, MAX_REQUEST_HEAD);
    if (total <= 0) {
        free(headbuf);
        return NULL;
    }

    char *body_start = strstr(headbuf, "\r\n\r\n");
    if (!body_start) {
        free(headbuf);
        return NULL;
    }
    *body_start = '\0';
    body_start += 4;
    size_t already_in_buffer = (size_t)total - (size_t)(body_start - headbuf);

    Request *req = parse_request(headbuf);
    if (!req) {
        free(headbuf);
        return NULL;
    }

    const char *cl = get_header_value(req, "Content-Length");
    if (cl) {
        long long len = strtoll(cl, NULL, 10);
        if (len < 0 || (size_t)len > MAX_BODY_SIZE) {
            free(headbuf);
            request_free(req);
            return NULL;
        }
        size_t body_len = (size_t)len;
        req->body = malloc(body_len + 1);
        if (!req->body) {
            free(headbuf);
            request_free(req);
            return NULL;
        }

        size_t copied = already_in_buffer < body_len ? already_in_buffer : body_len;
        if (copied > 0) memcpy(req->body, body_start, copied);

        size_t remaining = body_len - copied;
        size_t off = copied;
        while (remaining > 0) {
            ssize_t n = recv(sockfd, req->body + off, remaining, 0);
            if (n <= 0) {
                free(headbuf);
                request_free(req);
                return NULL;
            }
            off += (size_t)n;
            remaining -= (size_t)n;
        }
        req->body[body_len] = '\0';
        req->bodyLength = body_len;
    }

    free(headbuf);
    return req;
}

void request_free(Request *req) {
    if (!req) return;
    free(req->path);
    if (req->queryParams) {
        for (size_t i = 0; i < req->queryParamsCount; i++) {
            free(req->queryParams[i].key);
            free(req->queryParams[i].value);
        }
        free(req->queryParams);
    }
    if (req->headers) {
        for (size_t i = 0; i < req->headersCount; i++) {
            free(req->headers[i].key);
            free(req->headers[i].value);
        }
        free(req->headers);
    }
    free(req->body);
    free(req);
}

const char *get_query_param_value(const Request *request, const char *key) {
    if (!request || !key) return NULL;
    for (size_t i = 0; i < request->queryParamsCount; i++) {
        if (strcmp(request->queryParams[i].key, key) == 0) {
            return request->queryParams[i].value;
        }
    }
    return NULL;
}

const char *get_header_value(const Request *request, const char *name) {
    if (!request || !name) return NULL;
    for (size_t i = 0; i < request->headersCount; i++) {
        if (strcasecmp(request->headers[i].key, name) == 0) {
            return request->headers[i].value;
        }
    }
    return NULL;
}
