#ifndef ASYNC_AUDIO_PLAYER_LOG_H
#define ASYNC_AUDIO_PLAYER_LOG_H

typedef enum
{
    LOG_DEBUG = 0,  // Most verbose
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3   // Least verbose
} log_level_t;

// Set global log level
void log_set_level(const char* level);

// Log functions
void log_error(const char* format, ...);
void log_warn(const char* format, ...);
void log_info(const char* format, ...);
void log_debug(const char* format, ...);

#endif // ASYNC_AUDIO_PLAYER_LOG_H
