#pragma once

#include <stddef.h>

#define MAX_HEADERS 32

typedef struct {
    char *status_code;
    char *headers[MAX_HEADERS];
    size_t headersCount;
    char *data;
    size_t data_length;
} Response;

Response *response_new(const char *status_code);
int response_add_header(Response *r, const char *header_line);
int response_set_body(Response *r, const char *body);
int response_set_body_n(Response *r, const void *body, size_t length);
void response_free(Response *r);
