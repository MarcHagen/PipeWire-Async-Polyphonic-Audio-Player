#ifndef ASYNC_AUDIO_PLAYER_LOG_H
#define ASYNC_AUDIO_PLAYER_LOG_H

#include <stdio.h>

typedef enum {
    LOG_ERROR,
    LOG_INFO,
    LOG_DEBUG
} log_level_t;

// Set global log level
void log_set_level(const char *level);

// Log functions
void log_error(const char *format, ...);
void log_info(const char *format, ...);
void log_debug(const char *format, ...);

#endif // ASYNC_AUDIO_PLAYER_LOG_H
