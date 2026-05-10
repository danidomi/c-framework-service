#pragma once

#include <stddef.h>

#define MAX_HEADERS 32

/* Common HTTP status lines. Pass these to response_new() instead of
 * hand-typed strings to avoid drift across handlers. */
#define HTTP_OK                    "200 OK"
#define HTTP_CREATED               "201 Created"
#define HTTP_ACCEPTED              "202 Accepted"
#define HTTP_NO_CONTENT            "204 No Content"
#define HTTP_MOVED_PERMANENTLY     "301 Moved Permanently"
#define HTTP_FOUND                 "302 Found"
#define HTTP_SEE_OTHER             "303 See Other"
#define HTTP_NOT_MODIFIED          "304 Not Modified"
#define HTTP_TEMPORARY_REDIRECT    "307 Temporary Redirect"
#define HTTP_PERMANENT_REDIRECT    "308 Permanent Redirect"
#define HTTP_BAD_REQUEST           "400 Bad Request"
#define HTTP_UNAUTHORIZED          "401 Unauthorized"
#define HTTP_FORBIDDEN             "403 Forbidden"
#define HTTP_NOT_FOUND             "404 Not Found"
#define HTTP_METHOD_NOT_ALLOWED    "405 Method Not Allowed"
#define HTTP_CONFLICT              "409 Conflict"
#define HTTP_GONE                  "410 Gone"
#define HTTP_PAYLOAD_TOO_LARGE     "413 Payload Too Large"
#define HTTP_UNSUPPORTED_MEDIA     "415 Unsupported Media Type"
#define HTTP_UNPROCESSABLE_ENTITY  "422 Unprocessable Entity"
#define HTTP_TOO_MANY_REQUESTS     "429 Too Many Requests"
#define HTTP_INTERNAL_SERVER_ERROR "500 Internal Server Error"
#define HTTP_NOT_IMPLEMENTED       "501 Not Implemented"
#define HTTP_BAD_GATEWAY           "502 Bad Gateway"
#define HTTP_SERVICE_UNAVAILABLE   "503 Service Unavailable"
#define HTTP_GATEWAY_TIMEOUT       "504 Gateway Timeout"

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
