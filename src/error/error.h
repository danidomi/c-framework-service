#pragma once

typedef struct {
    char * message;
} Error;

Error * new(char * message);
