#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "log.h"

static log_level_t current_level = LOG_INFO;

// Convert string log level to enum
void log_set_level(const char *level) {
    if (!level) return;

    if (strcasecmp(level, "DEBUG") == 0) {
        current_level = LOG_DEBUG;
    } else if (strcasecmp(level, "INFO") == 0) {
        current_level = LOG_INFO;
    } else if (strcasecmp(level, "WARN") == 0) {
        current_level = LOG_WARN;
    } else if (strcasecmp(level, "ERROR") == 0) {
        current_level = LOG_ERROR;
    }
}

// Internal logging function
static void log_print(log_level_t level, const char *level_str, const char *format, va_list args) {
    if (level < current_level) return;

    time_t now;
    time(&now);
    char time_buf[26];
    ctime_r(&now, time_buf);
    time_buf[24] = '\0'; // Remove newline

    // Print to stderr for errors, stdout for others
    FILE *output = (level == LOG_ERROR) ? stderr : stdout;

    fprintf(output, "%s [%s] ", time_buf, level_str);
    vfprintf(output, format, args);
    fprintf(output, "\n");
    fflush(output);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_print(LOG_ERROR, "ERROR", format, args);
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_print(LOG_WARN, "WARN", format, args);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_print(LOG_INFO, "INFO", format, args);
    va_end(args);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_print(LOG_DEBUG, "DEBUG", format, args);
    va_end(args);
}
