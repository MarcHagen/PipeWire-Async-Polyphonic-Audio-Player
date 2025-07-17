#ifndef ASYNC_AUDIO_PLAYER_TYPES_H
#define ASYNC_AUDIO_PLAYER_TYPES_H

#include <stdbool.h>
#include <pipewire/pipewire.h>

// Track states
typedef enum {
    TRACK_STATE_STOPPED,
    TRACK_STATE_PLAYING,
    TRACK_STATE_ERROR
} track_state_t;

// Output mapping configuration
typedef struct {
    char *device;
    int *mapping;        // Array of AUX channel numbers
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

// Active track instance
typedef struct {
    track_config_t *config;
    track_state_t state;
    struct pw_stream *stream;    // Pipewire stream
    void *buffer;               // Audio buffer
    size_t buffer_size;
    bool should_stop;          // Flag for graceful shutdown
    pthread_t thread;          // Playback thread
} track_instance_t;

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
} global_config_t;

#endif // ASYNC_AUDIO_PLAYER_TYPES_H
