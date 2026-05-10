#pragma once

#include <stddef.h>

#define HTTP_HEAD_BUF 8192

typedef enum {
    METHOD_UNKNOWN = -1,
    GET = 0,
    POST,
    HEAD,
    PATCH,
    PUT,
    OPTIONS,
    DELETE
} Method;

typedef struct {
    char *key;
    char *value;
} KeyValuePair;

typedef struct {
    Method method;
    char *path;
    int http_minor;
    KeyValuePair *queryParams;
    size_t queryParamsCount;
    KeyValuePair *headers;
    size_t headersCount;
    char *body;
    size_t bodyLength;
} Request;

typedef struct {
    int sockfd;
    char buf[HTTP_HEAD_BUF];
    size_t pos;
    size_t end;
} ConnState;

void conn_state_init(ConnState *c, int sockfd);

Request *parse_request(const char *raw);
Request *request_recv(ConnState *c);
void request_free(Request *req);

const char *get_query_param_value(const Request *request, const char *key);
const char *get_header_value(const Request *request, const char *name);
const char *method_name(Method m);

/* Append a key/value pair to the request's queryParams. The pair is
 * duplicated; caller retains ownership of its strings. Returns 0 on
 * success, -1 on failure (allocation, capacity, or null args).
 * Used by the router to expose matched path-param segments to handlers. */
int request_add_query_param(Request *req, const char *key, const char *value);
