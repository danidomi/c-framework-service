#pragma once

#include <stddef.h>
#include <stdint.h>

/* Lightweight Prometheus-style metrics for the framework + apps.
 * Counters and gauges are atomic, safe to mutate from worker threads.
 *
 * Disabled at runtime by setting METRICS_ENABLED=0 (also accepts
 * false, no, off — case insensitive). When disabled:
 *   - metrics_record_request() is a no-op
 *   - metric_*_register() returns NULL
 *   - metric_*_inc/add/set on a NULL handle is a no-op (so app code
 *     does not need to special-case disabled vs enabled)
 *   - metrics_render_prometheus() returns NULL
 *   - metrics_is_enabled() returns 0
 */

void metrics_init(void);
int  metrics_is_enabled(void);

/* Built-in: bumped by the server after every dispatched request. */
void metrics_record_request(int status_code);

/* ------ Custom metrics (apps register their own) ------
 *
 * Register a metric ONCE, typically during startup, and keep the
 * returned handle in a static. Then mutate it from anywhere. The
 * handle is owned by the metrics module — do not free it.
 *
 * Limits: up to 32 counters and 32 gauges per process. Names should
 * follow Prometheus conventions (snake_case, _total suffix for
 * counters). */

typedef struct MetricCounter MetricCounter;
typedef struct MetricGauge   MetricGauge;

MetricCounter *metric_counter_register(const char *name, const char *help);
void           metric_counter_inc(MetricCounter *c);
void           metric_counter_add(MetricCounter *c, uint64_t delta);

MetricGauge *metric_gauge_register(const char *name, const char *help);
void         metric_gauge_set(MetricGauge *g, double value);
void         metric_gauge_inc(MetricGauge *g);
void         metric_gauge_dec(MetricGauge *g);

/* Render every metric (built-in + custom) as Prometheus text-format.
 * Caller frees. Writes byte length to *out_len if non-NULL.
 * Returns NULL on OOM or when metrics are disabled. */
char *metrics_render_prometheus(size_t *out_len);
