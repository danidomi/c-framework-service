# c-framework-service

[![GitHub release](https://img.shields.io/github/release/danidomi/c-framework-service.svg)](https://github.com/danidomi/c-framework-service/releases)

A small C framework for building HTTP microservices. Single binary, no
external runtime dependencies (other than libc + pthreads), distributed as a
prebuilt object file via [cdeps](https://github.com/danidomi/cdeps) so apps
just link against it.

## What's in the box

- **HTTP/1.1 server** with keep-alive, chunked-body decoding, per-socket
  timeouts, connection cap (256), graceful SIGINT/SIGTERM drain.
- **Router** with method + path matching and `:param` path parameters
  (`/cats/:id` → handler reads `id` via `get_query_param_value`).
- **Request** parsing: query string, headers, body. URL decoding (`%XX`, `+`).
- **Response** building with named HTTP status macros (`HTTP_OK`,
  `HTTP_CREATED`, `HTTP_NOT_FOUND`, `HTTP_INTERNAL_SERVER_ERROR`, etc.).
- **Logger** (`DEBUG`, `INFO`, `WARNING`, `ERROR`).
- **Health & metrics**: `/healthz`, `/readyz`, `/metrics` (Prometheus text
  format), runnable on a dedicated admin port so they don't share traffic
  with the public API.
- **Custom metrics API**: register counters and gauges from app code, exposed
  on the same `/metrics` endpoint. Atomic, safe to mutate from worker threads.
- **Error** type with `error_new` / `error_free`.
- **Database config** loader for property files.

## Install

Add to your project's `c.deps`:

```
github.com/danidomi/c-framework-service v0.4.0
```

Then:

```shell
cdeps install
```

This downloads the platform-matched zip into `deps/c-framework-service/<OS>_<ARCH>/`
(e.g. `Linux_x86_64`). Most projects flatten that one level so the include
path resolves at the natural location:

```shell
for d in deps/*/Linux_* deps/*/Darwin_*; do
    [ -d "$d" ] || continue
    mv "$d"/* "$(dirname "$d")/"
    rmdir "$d"
done
```

After flattening you get `deps/c-framework-service/c-framework-service.o` and
`deps/c-framework-service/{request,response,router,...}/*.h`. Supported
platforms: Linux x86_64/i386/aarch64/armv7l and Darwin arm64/x86_64.

## Quick start

```c
#include <c-framework-service/health/health.h>
#include <c-framework-service/logger/logger.h>
#include <c-framework-service/router/router.h>
#include <c-framework-service/server/server.h>

static Response *handle_hello(Request *req) {
    (void)req;
    Response *r = response_new(HTTP_OK);
    response_add_header(r, "Content-Type: application/json; charset=utf-8");
    response_set_body(r, "{\"hello\":\"world\"}");
    return r;
}

int main(void) {
    log_message(INFO, "starting");

    route_register(GET, "/hello", handle_hello);
    health_start_admin_server(0);   /* 0 = $ADMIN_PORT or 9090 */

    return server_run(port_from_env(PORT));
}
```

Build:

```shell
cc -Ideps -o myservice main.c deps/c-framework-service/c-framework-service.o -lpthread
PORT=8080 ./myservice
```

## Routing with path params

```c
route_register(GET,    "/cats",     handle_list);
route_register(POST,   "/cats",     handle_create);
route_register(GET,    "/cats/:id", handle_get);
route_register(PUT,    "/cats/:id", handle_update);
route_register(DELETE, "/cats/:id", handle_delete);
```

Inside a handler, the matched path-param is bound onto the request's query
params, so the same getter retrieves both:

```c
const char *id = get_query_param_value(req, "id");
```

## Response status macros

```c
response_new(HTTP_OK);                  /* 200 OK */
response_new(HTTP_CREATED);             /* 201 Created */
response_new(HTTP_NO_CONTENT);          /* 204 No Content */
response_new(HTTP_BAD_REQUEST);         /* 400 Bad Request */
response_new(HTTP_NOT_FOUND);           /* 404 Not Found */
response_new(HTTP_INTERNAL_SERVER_ERROR);
/* ...also: HTTP_UNAUTHORIZED, HTTP_FORBIDDEN, HTTP_METHOD_NOT_ALLOWED,
   HTTP_CONFLICT, HTTP_UNPROCESSABLE_ENTITY, HTTP_TOO_MANY_REQUESTS,
   HTTP_BAD_GATEWAY, HTTP_SERVICE_UNAVAILABLE, etc. — see response.h */
```

## Health and metrics

Two ways to expose `/healthz`, `/readyz`, `/metrics`:

```c
/* On a dedicated admin port (recommended for production —
   keeps /metrics off your public traffic). */
health_start_admin_server(0);    /* 0 -> $ADMIN_PORT, default 9090 */
health_start_admin_server(9090); /* explicit port */

/* Or on the main server's port (simpler, public): */
health_register_default_routes();
```

Optional readiness check:

```c
static int my_readiness(void) {
    return database_is_connected() ? 1 : 0;
}

health_set_readiness_check(my_readiness);
```

## Custom metrics

```c
#include <c-framework-service/metrics/metrics.h>

static MetricCounter *requests_handled;
static MetricGauge   *queue_depth;

void app_init(void) {
    requests_handled = metric_counter_register(
        "requests_handled_total", "Requests handled since startup");
    queue_depth = metric_gauge_register(
        "queue_depth", "Items waiting in queue");
}

/* Anywhere */
metric_counter_inc(requests_handled);
metric_counter_add(requests_handled, 5);
metric_gauge_set(queue_depth, 12.0);
metric_gauge_inc(queue_depth);
metric_gauge_dec(queue_depth);
```

Returns `NULL` when metrics are disabled — the inc/set helpers no-op on
`NULL`, so app code doesn't have to special-case it. Limits: 32 counters and
32 gauges per process.

## Environment variables

| Variable          | Default | Effect                                                               |
|-------------------|---------|----------------------------------------------------------------------|
| `PORT`            | 8080    | Main HTTP listener (the value passed to `port_from_env(PORT)`)       |
| `ADMIN_PORT`      | 9090    | Used by `health_start_admin_server(0)` when no explicit port given   |
| `METRICS_ENABLED` | true    | Set `0`/`false`/`no`/`off` to disable collection and `/metrics`      |

## Built-in metrics

Always emitted (when enabled) on `/metrics`:

```
# HELP service_http_requests_total Total HTTP requests handled
# TYPE service_http_requests_total counter
service_http_requests_total 42

# HELP service_http_requests_by_status HTTP requests handled by status code
# TYPE service_http_requests_by_status counter
service_http_requests_by_status{status="200"} 38
service_http_requests_by_status{status="404"} 4

# HELP service_uptime_seconds Process uptime in seconds
# TYPE service_uptime_seconds gauge
service_uptime_seconds 1234
```

Followed by every counter / gauge the app registered via the metrics API.

## Building from source

```shell
make all                          # builds bin/server (the included demo)
make release                      # builds release/<OS>_<ARCH>/{c-framework-service.o, headers}
                                  # and a corresponding .zip ready to upload to GitHub
```

To produce all release zips listed in `c.rels`, use
[crels](https://github.com/danidomi/crels) — it iterates the file and shells
out to Docker for each Linux target.

## License

MIT.
