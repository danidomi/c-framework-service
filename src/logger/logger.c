#include "logger.h"

#include <stdlib.h>

static void get_current_time(char *timestamp, size_t size) {
    time_t rawTime;
    struct tm timeInfo;

    time(&rawTime);
    localtime_r(&rawTime, &timeInfo);
    strftime(timestamp, size, "%Y-%m-%d %H:%M:%S", &timeInfo);
}

static const char *level_label(LogLevel level) {
    switch (level) {
        case DEBUG:   return "DEBUG";
        case INFO:    return "INFO";
        case WARNING: return "WARNING";
        case ERROR:   return "ERROR";
        default:      return "UNKNOWN";
    }
}

void log_message(LogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);

    char timestamp[20];
    get_current_time(timestamp, sizeof(timestamp));

    int close_after = 0;
    FILE *out;
    if (strcmp(LOG_FILE, "stderr") == 0) {
        out = stderr;
    } else {
        out = fopen(LOG_FILE, "a");
        if (!out) {
            out = stderr;
        } else {
            close_after = 1;
        }
    }

    fprintf(out, "[%s] [%s] ", timestamp, level_label(level));
    vfprintf(out, format, args);
    fputc('\n', out);
    fflush(out);

    if (close_after) fclose(out);
    va_end(args);
}
