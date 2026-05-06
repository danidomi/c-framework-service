#include "error.h"

#include <stdlib.h>
#include <string.h>

Error *error_new(const char *message) {
    Error *e = malloc(sizeof(Error));
    if (!e) return NULL;
    e->message = message ? strdup(message) : NULL;
    if (message && !e->message) {
        free(e);
        return NULL;
    }
    return e;
}

void error_free(Error *e) {
    if (!e) return;
    free(e->message);
    free(e);
}
