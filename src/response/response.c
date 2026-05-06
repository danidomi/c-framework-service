#include "response.h"

#include <stdlib.h>
#include <string.h>

Response *response_new(const char *status_code) {
    Response *r = calloc(1, sizeof(Response));
    if (!r) return NULL;
    r->status_code = strdup(status_code ? status_code : "200 OK");
    if (!r->status_code) {
        free(r);
        return NULL;
    }
    return r;
}

int response_add_header(Response *r, const char *header_line) {
    if (!r || !header_line) return -1;
    if (r->headersCount >= MAX_HEADERS) return -1;
    char *copy = strdup(header_line);
    if (!copy) return -1;
    r->headers[r->headersCount++] = copy;
    return 0;
}

int response_set_body(Response *r, const char *body) {
    if (!r) return -1;
    return response_set_body_n(r, body, body ? strlen(body) : 0);
}

int response_set_body_n(Response *r, const void *body, size_t length) {
    if (!r) return -1;
    free(r->data);
    r->data = NULL;
    r->data_length = 0;
    if (length == 0) return 0;
    r->data = malloc(length + 1);
    if (!r->data) return -1;
    if (body) memcpy(r->data, body, length);
    r->data[length] = '\0';
    r->data_length = length;
    return 0;
}

void response_free(Response *r) {
    if (!r) return;
    free(r->status_code);
    for (size_t i = 0; i < r->headersCount; i++) {
        free(r->headers[i]);
    }
    free(r->data);
    free(r);
}
