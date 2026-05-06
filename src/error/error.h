#pragma once

typedef struct {
    char *message;
} Error;

Error *error_new(const char *message);
void error_free(Error *e);
