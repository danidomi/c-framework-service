#include "metrics.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define STATUS_RANGE 600
#define MAX_COUNTERS 32
#define MAX_GAUGES   32
#define MAX_METRIC_NAME 64
#define MAX_METRIC_HELP 128

struct MetricCounter {
    char name[MAX_METRIC_NAME];
    char help[MAX_METRIC_HELP];
    _Atomic uint64_t value;
};

struct MetricGauge {
    char name[MAX_METRIC_NAME];
    char help[MAX_METRIC_HELP];
    _Atomic uint64_t bits;   /* IEEE 754 bits of a double */
};

static _Atomic uint64_t requests_total = 0;
static _Atomic uint64_t requests_by_status[STATUS_RANGE];
static time_t start_time = 0;
static int enabled = -1;

static struct MetricCounter counters[MAX_COUNTERS];
static struct MetricGauge   gauges[MAX_GAUGES];
static size_t n_counters = 0;
static size_t n_gauges   = 0;
static pthread_mutex_t reg_mu = PTHREAD_MUTEX_INITIALIZER;

static int parse_bool(const char *v, int default_value) {
    if (!v || !*v) return default_value;
    if (!strcasecmp(v, "0") || !strcasecmp(v, "false") ||
        !strcasecmp(v, "no") || !strcasecmp(v, "off")) return 0;
    return 1;
}

static double bits_to_double(uint64_t bits) {
    double d;
    memcpy(&d, &bits, sizeof(d));
    return d;
}
static uint64_t double_to_bits(double d) {
    uint64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    return bits;
}

void metrics_init(void) {
    if (start_time != 0) return;
    enabled = parse_bool(getenv("METRICS_ENABLED"), 1);
    start_time = time(NULL);
}

int metrics_is_enabled(void) {
    if (enabled < 0) metrics_init();
    return enabled;
}

void metrics_record_request(int status_code) {
    if (!metrics_is_enabled()) return;
    atomic_fetch_add(&requests_total, 1);
    if (status_code >= 0 && status_code < STATUS_RANGE) {
        atomic_fetch_add(&requests_by_status[status_code], 1);
    }
}

MetricCounter *metric_counter_register(const char *name, const char *help) {
    if (!metrics_is_enabled() || !name) return NULL;
    pthread_mutex_lock(&reg_mu);
    if (n_counters >= MAX_COUNTERS) {
        pthread_mutex_unlock(&reg_mu);
        return NULL;
    }
    MetricCounter *c = &counters[n_counters++];
    snprintf(c->name, sizeof(c->name), "%s", name);
    snprintf(c->help, sizeof(c->help), "%s", help ? help : "");
    atomic_store(&c->value, 0);
    pthread_mutex_unlock(&reg_mu);
    return c;
}

void metric_counter_inc(MetricCounter *c) {
    if (!c) return;
    atomic_fetch_add(&c->value, 1);
}

void metric_counter_add(MetricCounter *c, uint64_t delta) {
    if (!c) return;
    atomic_fetch_add(&c->value, delta);
}

MetricGauge *metric_gauge_register(const char *name, const char *help) {
    if (!metrics_is_enabled() || !name) return NULL;
    pthread_mutex_lock(&reg_mu);
    if (n_gauges >= MAX_GAUGES) {
        pthread_mutex_unlock(&reg_mu);
        return NULL;
    }
    MetricGauge *g = &gauges[n_gauges++];
    snprintf(g->name, sizeof(g->name), "%s", name);
    snprintf(g->help, sizeof(g->help), "%s", help ? help : "");
    atomic_store(&g->bits, double_to_bits(0.0));
    pthread_mutex_unlock(&reg_mu);
    return g;
}

void metric_gauge_set(MetricGauge *g, double value) {
    if (!g) return;
    atomic_store(&g->bits, double_to_bits(value));
}

void metric_gauge_inc(MetricGauge *g) {
    if (!g) return;
    uint64_t cur, new_b;
    do {
        cur = atomic_load(&g->bits);
        new_b = double_to_bits(bits_to_double(cur) + 1.0);
    } while (!atomic_compare_exchange_weak(&g->bits, &cur, new_b));
}

void metric_gauge_dec(MetricGauge *g) {
    if (!g) return;
    uint64_t cur, new_b;
    do {
        cur = atomic_load(&g->bits);
        new_b = double_to_bits(bits_to_double(cur) - 1.0);
    } while (!atomic_compare_exchange_weak(&g->bits, &cur, new_b));
}

static int ensure_cap(char **buf, size_t *cap, size_t pos, size_t need) {
    if (pos + need <= *cap) return 0;
    size_t new_cap = *cap;
    while (pos + need > new_cap) new_cap *= 2;
    char *grown = realloc(*buf, new_cap);
    if (!grown) return -1;
    *buf = grown;
    *cap = new_cap;
    return 0;
}

char *metrics_render_prometheus(size_t *out_len) {
    if (!metrics_is_enabled()) return NULL;

    size_t cap = 4096;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;

    pos += snprintf(buf + pos, cap - pos,
        "# HELP service_http_requests_total Total HTTP requests handled\n"
        "# TYPE service_http_requests_total counter\n"
        "service_http_requests_total %llu\n",
        (unsigned long long)atomic_load(&requests_total));

    pos += snprintf(buf + pos, cap - pos,
        "# HELP service_http_requests_by_status HTTP requests handled by status code\n"
        "# TYPE service_http_requests_by_status counter\n");

    for (int code = 100; code < STATUS_RANGE; code++) {
        uint64_t v = atomic_load(&requests_by_status[code]);
        if (v == 0) continue;
        if (ensure_cap(&buf, &cap, pos, 128) != 0) { free(buf); return NULL; }
        pos += snprintf(buf + pos, cap - pos,
            "service_http_requests_by_status{status=\"%d\"} %llu\n",
            code, (unsigned long long)v);
    }

    double uptime = (double)(time(NULL) - start_time);
    if (ensure_cap(&buf, &cap, pos, 256) != 0) { free(buf); return NULL; }
    pos += snprintf(buf + pos, cap - pos,
        "# HELP service_uptime_seconds Process uptime in seconds\n"
        "# TYPE service_uptime_seconds gauge\n"
        "service_uptime_seconds %.0f\n", uptime);

    /* Custom counters */
    pthread_mutex_lock(&reg_mu);
    size_t nc = n_counters, ng = n_gauges;
    pthread_mutex_unlock(&reg_mu);

    for (size_t i = 0; i < nc; i++) {
        MetricCounter *c = &counters[i];
        if (ensure_cap(&buf, &cap, pos, MAX_METRIC_NAME + MAX_METRIC_HELP + 128) != 0) { free(buf); return NULL; }
        pos += snprintf(buf + pos, cap - pos,
            "# HELP %s %s\n# TYPE %s counter\n%s %llu\n",
            c->name, c->help, c->name, c->name,
            (unsigned long long)atomic_load(&c->value));
    }

    for (size_t i = 0; i < ng; i++) {
        MetricGauge *g = &gauges[i];
        if (ensure_cap(&buf, &cap, pos, MAX_METRIC_NAME + MAX_METRIC_HELP + 128) != 0) { free(buf); return NULL; }
        pos += snprintf(buf + pos, cap - pos,
            "# HELP %s %s\n# TYPE %s gauge\n%s %g\n",
            g->name, g->help, g->name, g->name,
            bits_to_double(atomic_load(&g->bits)));
    }

    if (out_len) *out_len = pos;
    return buf;
}
