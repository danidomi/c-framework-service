#pragma once

#include <stddef.h>

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
    KeyValuePair *queryParams;
    size_t queryParamsCount;
    KeyValuePair *headers;
    size_t headersCount;
    char *body;
    size_t bodyLength;
} Request;

Request *parse_request(const char *raw);
Request *request_recv(int sockfd);
void request_free(Request *req);

const char *get_query_param_value(const Request *request, const char *key);
const char *get_header_value(const Request *request, const char *name);
const char *method_name(Method m);
