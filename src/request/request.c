#include "request.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

#define MAX_BODY_SIZE (8 * 1024 * 1024)
#define MAX_QUERY_PARAMS 64
#define MAX_HEADERS_IN 64
#define CHUNK_LINE_MAX 256

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

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static char *url_decode(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '+') {
            out[j++] = ' ';
        } else if (s[i] == '%' && i + 2 < n) {
            int hi = hex_val(s[i + 1]);
            int lo = hex_val(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out[j++] = (char)((hi << 4) | lo);
                i += 2;
            } else {
                out[j++] = s[i];
            }
        } else {
            out[j++] = s[i];
        }
    }
    out[j] = '\0';
    return out;
}

static int parse_query_string(Request *request, char *queryStart) {
    request->queryParams = calloc(MAX_QUERY_PARAMS, sizeof(KeyValuePair));
    if (!request->queryParams) return -1;

    char *saveptr = NULL;
    char *tok = strtok_r(queryStart, "&", &saveptr);
    while (tok && request->queryParamsCount < MAX_QUERY_PARAMS) {
        char *eq = strchr(tok, '=');
        const char *raw_key;
        const char *raw_val;
        if (eq) {
            *eq = '\0';
            raw_key = tok;
            raw_val = eq + 1;
        } else {
            raw_key = tok;
            raw_val = "";
        }
        char *k = url_decode(raw_key);
        char *v = url_decode(raw_val);
        if (!k || !v) {
            free(k);
            free(v);
            return -1;
        }
        request->queryParams[request->queryParamsCount].key = k;
        request->queryParams[request->queryParamsCount].value = v;
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
    request->http_minor = 1;

    char *copy = strdup(raw);
    if (!copy) {
        free(request);
        return NULL;
    }

    char *p = copy;
    char *line_end = strstr(p, "\r\n");
    if (!line_end) goto fail;
    *line_end = '\0';
    char *request_line = p;
    p = line_end + 2;

    char *m_end = strchr(request_line, ' ');
    if (!m_end) goto fail;
    *m_end = '\0';
    char *method_str = request_line;
    char *path_start = m_end + 1;

    char *p_end = strchr(path_start, ' ');
    char *version_str = NULL;
    if (p_end) {
        *p_end = '\0';
        version_str = p_end + 1;
    }

    request->method = method_from_string(method_str);
    if (request->method == METHOD_UNKNOWN) goto fail;

    if (version_str) {
        if (strcmp(version_str, "HTTP/1.1") == 0) {
            request->http_minor = 1;
        } else if (strcmp(version_str, "HTTP/1.0") == 0) {
            request->http_minor = 0;
        } else {
            goto fail;
        }
    }

    char *query = strchr(path_start, '?');
    if (query) {
        *query = '\0';
        query++;
    }

    request->path = strdup(path_start);
    if (!request->path) goto fail;

    if (query && *query) {
        if (parse_query_string(request, query) != 0) goto fail;
    }

    request->headers = calloc(MAX_HEADERS_IN, sizeof(KeyValuePair));
    if (!request->headers) goto fail;

    while (*p) {
        line_end = strstr(p, "\r\n");
        if (!line_end) break;
        if (line_end == p) break;
        *line_end = '\0';
        if (request->headersCount >= MAX_HEADERS_IN) {
            p = line_end + 2;
            continue;
        }
        char *colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
            char *value = trim(colon + 1);
            char *key = trim(p);
            request->headers[request->headersCount].key = strdup(key);
            request->headers[request->headersCount].value = strdup(value);
            request->headersCount++;
        }
        p = line_end + 2;
    }

    free(copy);
    return request;

fail:
    free(copy);
    request_free(request);
    return NULL;
}

void conn_state_init(ConnState *c, int sockfd) {
    c->sockfd = sockfd;
    c->pos = 0;
    c->end = 0;
}

static int conn_compact(ConnState *c) {
    if (c->pos > 0) {
        memmove(c->buf, c->buf + c->pos, c->end - c->pos);
        c->end -= c->pos;
        c->pos = 0;
    }
    return c->end < sizeof(c->buf) ? 0 : -1;
}

static ssize_t fill_head(ConnState *c) {
    while (1) {
        if (c->end >= c->pos + 4) {
            for (size_t i = c->pos; i + 3 < c->end; i++) {
                if (c->buf[i] == '\r' && c->buf[i + 1] == '\n' &&
                    c->buf[i + 2] == '\r' && c->buf[i + 3] == '\n') {
                    return (ssize_t)(i + 4 - c->pos);
                }
            }
        }
        if (c->end == sizeof(c->buf)) {
            if (conn_compact(c) != 0) return -1;
        }
        ssize_t n = recv(c->sockfd, c->buf + c->end, sizeof(c->buf) - c->end, 0);
        if (n <= 0) return n < 0 ? -1 : 0;
        c->end += (size_t)n;
    }
}

static int read_exact(ConnState *c, void *dst, size_t n) {
    char *d = (char *)dst;
    size_t copied = 0;
    if (c->end > c->pos) {
        size_t avail = c->end - c->pos;
        size_t take = avail < n ? avail : n;
        memcpy(d, c->buf + c->pos, take);
        c->pos += take;
        copied = take;
    }
    while (copied < n) {
        ssize_t r = recv(c->sockfd, d + copied, n - copied, 0);
        if (r <= 0) return -1;
        copied += (size_t)r;
    }
    return 0;
}

static int read_line(ConnState *c, char *out, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char ch;
        if (read_exact(c, &ch, 1) != 0) return -1;
        out[n++] = ch;
        if (n >= 2 && out[n - 2] == '\r' && out[n - 1] == '\n') {
            out[n - 2] = '\0';
            return 0;
        }
    }
    return -1;
}

static int read_chunked_body(ConnState *c, Request *req) {
    char line[CHUNK_LINE_MAX];
    char *body = NULL;
    size_t total = 0;
    size_t cap = 0;

    for (;;) {
        if (read_line(c, line, sizeof(line)) != 0) goto fail;
        char *semi = strchr(line, ';');
        if (semi) *semi = '\0';
        char *endp = NULL;
        unsigned long size = strtoul(line, &endp, 16);
        if (endp == line) goto fail;

        if (size == 0) {
            for (;;) {
                if (read_line(c, line, sizeof(line)) != 0) goto fail;
                if (line[0] == '\0') break;
            }
            break;
        }
        if (total + size > MAX_BODY_SIZE) goto fail;

        size_t need = total + size + 1;
        if (need > cap) {
            size_t new_cap = cap == 0 ? 4096 : cap;
            while (new_cap < need) new_cap *= 2;
            char *nb = realloc(body, new_cap);
            if (!nb) goto fail;
            body = nb;
            cap = new_cap;
        }
        if (read_exact(c, body + total, size) != 0) goto fail;
        total += size;

        char crlf[2];
        if (read_exact(c, crlf, 2) != 0 || crlf[0] != '\r' || crlf[1] != '\n') goto fail;
    }

    if (!body) {
        body = malloc(1);
        if (!body) return -1;
    }
    body[total] = '\0';
    req->body = body;
    req->bodyLength = total;
    return 0;

fail:
    free(body);
    return -1;
}

Request *request_recv(ConnState *c) {
    ssize_t head_len = fill_head(c);
    if (head_len <= 0) return NULL;

    char saved = c->buf[c->pos + head_len];
    c->buf[c->pos + head_len] = '\0';
    Request *req = parse_request(c->buf + c->pos);
    c->buf[c->pos + head_len] = saved;
    if (!req) return NULL;
    c->pos += (size_t)head_len;

    const char *te = get_header_value(req, "Transfer-Encoding");
    if (te && strcasecmp(te, "chunked") == 0) {
        if (read_chunked_body(c, req) != 0) {
            request_free(req);
            return NULL;
        }
        return req;
    }

    const char *cl = get_header_value(req, "Content-Length");
    if (cl) {
        long long len = strtoll(cl, NULL, 10);
        if (len < 0 || (size_t)len > MAX_BODY_SIZE) {
            request_free(req);
            return NULL;
        }
        size_t body_len = (size_t)len;
        req->body = malloc(body_len + 1);
        if (!req->body) {
            request_free(req);
            return NULL;
        }
        if (body_len > 0 && read_exact(c, req->body, body_len) != 0) {
            request_free(req);
            return NULL;
        }
        req->body[body_len] = '\0';
        req->bodyLength = body_len;
    }

    return req;
}

int request_add_query_param(Request *req, const char *key, const char *value) {
    if (!req || !key || !value) return -1;
    if (!req->queryParams) {
        req->queryParams = calloc(MAX_QUERY_PARAMS, sizeof(KeyValuePair));
        if (!req->queryParams) return -1;
    }
    if (req->queryParamsCount >= MAX_QUERY_PARAMS) return -1;
    char *k = strdup(key);
    char *v = strdup(value);
    if (!k || !v) { free(k); free(v); return -1; }
    req->queryParams[req->queryParamsCount].key = k;
    req->queryParams[req->queryParamsCount].value = v;
    req->queryParamsCount++;
    return 0;
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
