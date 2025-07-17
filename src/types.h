#ifndef ASYNC_AUDIO_PLAYER_TYPES_H
#define ASYNC_AUDIO_PLAYER_TYPES_H

#include <stdbool.h>
#include <pipewire/pipewire.h>

// Signal handling states
typedef enum {
    SIGNAL_NONE,
    SIGNAL_RELOAD,
    SIGNAL_SHUTDOWN
} signal_state_t;

// Track states
typedef enum {
    TRACK_STATE_STOPPED,
    TRACK_STATE_PLAYING,
    TRACK_STATE_ERROR,
    TRACK_STATE_CONNECTING,
    TRACK_STATE_DISCONNECTED
} track_state_t;

// Stream error info
typedef struct {
    char *message;
    int code;
} stream_error_t;

// Channel position mapping
typedef struct {
    const char *name;           // Channel name (e.g., "FL", "FR", "AUX0")
    uint32_t position;          // SPA_AUDIO_CHANNEL position value
} channel_map_t;

// Output mapping configuration
typedef struct {
    char *device;
    char **mapping;      // Array of port names (e.g., "FL", "FR", "AUX0")
    int mapping_count;   // Number of channels in mapping
} output_config_t;

// Track configuration
typedef struct {
    char *id;           // Unique track identifier
    char *file_path;    // Path to WAV file
    bool loop;          // Loop flag
    float volume;       // Volume level (0.0 - 1.0)
    output_config_t output;
} track_config_t;

#include "audio_file.h"

// Active track instance
typedef struct {
    track_config_t *config;
    track_state_t state;
    struct pw_stream *stream;    // Pipewire stream
    audio_file_t *audio_file;   // Audio file handler
    bool should_stop;          // Flag for graceful shutdown
    pthread_t thread;          // Playback thread
    stream_error_t error;      // Stream error information
    uint32_t target_id;       // Target node ID for connection
    bool is_connected;        // Stream connection state
} track_instance_t;

struct mqtt_client_ctx;  // Forward declaration

// Global configuration
typedef struct {
    struct {
        char *level;
    } logging;

    struct {
        bool enabled;
        char *broker;
        char *client_id;
        char *username;
        char *password;
        char *topic_prefix;
    } mqtt;

    struct {
        int sample_rate;
    } audio;

    track_config_t *tracks;
    int track_count;
    struct mqtt_client_ctx *mqtt_ctx;  // MQTT context for status updates
} global_config_t;

#endif // ASYNC_AUDIO_PLAYER_TYPES_H
