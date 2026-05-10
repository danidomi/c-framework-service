#pragma once

#include "../request/request.h"
#include "../response/response.h"

/* Optional readiness check. Return 1 if the service is ready to handle
 * traffic, 0 if it is not (e.g. database still connecting). When no
 * check is set, /readyz always reports ready. */
typedef int (*ReadinessCheck)(void);
void health_set_readiness_check(ReadinessCheck check);

/* Register the conventional probe + scrape endpoints on the MAIN server:
 *   GET /healthz  -> liveness  (always 200 if process is responding)
 *   GET /readyz   -> readiness (200 or 503 based on the check above)
 *   GET /metrics  -> Prometheus text-format metrics
 *
 * Use this when you don't mind exposing /metrics on the same port as
 * your app traffic. For production you'll usually want the admin server
 * on a separate port — see health_start_admin_server() below. */
void health_register_default_routes(void);

/* Spawn a background thread that serves /healthz, /readyz, /metrics on
 * a dedicated port. Anything else returns 404. Connection: close per
 * request — admin scrapes are infrequent.
 *
 * port == 0  -> read ADMIN_PORT env var, fall back to 9090.
 * Otherwise  -> bind to the given port literally.
 *
 * Call this BEFORE server_run() so the admin endpoints come up first.
 * The thread is detached and runs until the process exits. */
void health_start_admin_server(int port);

/* Handlers exposed in case an app wants to mount them at custom paths. */
Response *handle_healthz(Request *req);
Response *handle_readyz(Request *req);
Response *handle_metrics(Request *req);
