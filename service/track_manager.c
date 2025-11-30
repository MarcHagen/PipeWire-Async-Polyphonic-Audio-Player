#include "track_manager.h"
#include "log.h"
#include <math.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TRACKS 32
#define BUFFER_SIZE 4096

#include <stdint.h>
#include <spa/param/audio/raw.h>

struct track_manager_ctx {
    global_config_t *config;
    track_instance_t tracks[MAX_TRACKS];
    int active_tracks;
    struct pw_context *pw_context;
    struct pw_main_loop *pw_loop;
    bool initialized;
};

// Standard channel position mapping
static const struct channel_position {
    const char *name;
    enum spa_audio_channel position;
} spa_string_map[] = {
        {"MONO", SPA_AUDIO_CHANNEL_MONO},
        {"FL",   SPA_AUDIO_CHANNEL_FL},
        {"FR",   SPA_AUDIO_CHANNEL_FR},
        {"FC",   SPA_AUDIO_CHANNEL_FC},
        {"LFE",  SPA_AUDIO_CHANNEL_LFE},
        {"SL",   SPA_AUDIO_CHANNEL_SL},
        {"SR",   SPA_AUDIO_CHANNEL_SR},
        {"FLC",  SPA_AUDIO_CHANNEL_FLC},
        {"FRC",  SPA_AUDIO_CHANNEL_FRC},
        {"RC",   SPA_AUDIO_CHANNEL_RC},
        {"RL",   SPA_AUDIO_CHANNEL_RL},
        {"RR",   SPA_AUDIO_CHANNEL_RR},
        {"TC",   SPA_AUDIO_CHANNEL_TC},
        {"TFL",  SPA_AUDIO_CHANNEL_TFL},
        {"TFC",  SPA_AUDIO_CHANNEL_TFC},
        {"TFR",  SPA_AUDIO_CHANNEL_TFR},
        {"TRL",  SPA_AUDIO_CHANNEL_TRL},
        {"TRC",  SPA_AUDIO_CHANNEL_TRC},
        {"TRR",  SPA_AUDIO_CHANNEL_TRR},
        {"RLC",  SPA_AUDIO_CHANNEL_RLC},
        {"RRC",  SPA_AUDIO_CHANNEL_RRC},
        {"FLW",  SPA_AUDIO_CHANNEL_FLW},
        {"FRW",  SPA_AUDIO_CHANNEL_FRW},
        {"LFE2", SPA_AUDIO_CHANNEL_LFE2},
        {"FLH",  SPA_AUDIO_CHANNEL_FLH},
        {"FCH",  SPA_AUDIO_CHANNEL_FCH},
        {"FRH",  SPA_AUDIO_CHANNEL_FRH},
        {"TFLC", SPA_AUDIO_CHANNEL_TFLC},
        {"TFRC", SPA_AUDIO_CHANNEL_TFRC},
        {"TSL",  SPA_AUDIO_CHANNEL_TSL},
        {"TSR",  SPA_AUDIO_CHANNEL_TSR},
        {"LLFE", SPA_AUDIO_CHANNEL_LLFE},
        {"RLFE", SPA_AUDIO_CHANNEL_RLFE},
        {"BC",   SPA_AUDIO_CHANNEL_BC},
        {"BLC",  SPA_AUDIO_CHANNEL_BLC},
        {"BRC",  SPA_AUDIO_CHANNEL_BRC},
        {"NA",   SPA_AUDIO_CHANNEL_NA},
};

// Implement the get_channel_position function in the .c file
enum spa_audio_channel get_channel_position(const char *port_name) {
    // Handle AUX channels
    if (strncmp(port_name, "AUX", 3) == 0) {
        char *endPtr;
        errno = 0; // Reset errno before the call
        const long aux_num = strtol(port_name + 3, &endPtr, 10);
        // Check for conversion errors
        if (errno != 0 || endPtr == port_name + 3 || *endPtr != '\0' ||
            aux_num < 0 || aux_num > 63) {
            return SPA_AUDIO_CHANNEL_UNKNOWN;
        }

        return SPA_AUDIO_CHANNEL_START_Aux + (int) aux_num;
    }

    // Look up spa enum for given config string
    for (size_t i = 0; i < sizeof(spa_string_map) / sizeof(spa_string_map[0]);
         i++) {
        if (strcmp(port_name, spa_string_map[i].name) == 0) {
            return spa_string_map[i].position;
        }
    }

    return SPA_AUDIO_CHANNEL_UNKNOWN;
}

// PipeWire stream callback
static void on_process(void *userdata) {
    track_instance_t *track = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *dst;

    if ((b = pw_stream_dequeue_buffer(track->stream)) == NULL) {
        log_error("Out of buffers");
        return;
    }

    buf = b->buffer;
    dst = buf->datas[0].data;
    if (dst == NULL)
        return;

    const size_t n_frames = buf->datas[0].maxsize / sizeof(float) / track->audio_file->info.channels;

    // Read audio data
    const size_t frames_read = audio_file_read(track->audio_file, dst, n_frames);

    if (frames_read < n_frames) {
        if (!track->audio_file->loop) {
            // End of file reached and not looping
            if (track->state != TRACK_STATE_STOPPED) {
                log_info("Track finished: %s", track->config->id);
                track->state = TRACK_STATE_STOPPED;
            }
        }
        // Fill remaining buffer with silence
        memset(
                dst + (frames_read * track->audio_file->info.channels),
                0,
                (n_frames - frames_read) * track->audio_file->info.channels *
                sizeof(float)
        );
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = track->audio_file->info.channels * sizeof(float);
    buf->datas[0].chunk->size = n_frames * track->audio_file->info.channels * sizeof(float);

    pw_stream_queue_buffer(track->stream, b);
}

static void on_stream_state_changed(
        void *userdata,
        enum pw_stream_state old,
        enum pw_stream_state state,
        const char *error
) {
    track_instance_t *track = userdata;

    log_debug(
            "Stream state changed from %s to %s",
            pw_stream_state_as_string(old),
            pw_stream_state_as_string(state)
    );

    switch (state) {
        case PW_STREAM_STATE_ERROR:
            track->state = TRACK_STATE_ERROR;
            if (track->error.message) {
                free(track->error.message);
            }
            track->error.message = error ? strdup(error) : strdup("Unknown error");
            log_error("Stream error: %s", track->error.message);
            break;

        case PW_STREAM_STATE_CONNECTING:
            track->state = TRACK_STATE_CONNECTING;
            break;

        case PW_STREAM_STATE_PAUSED:
            if (track->state == TRACK_STATE_PLAYING) {
                log_info("Stream paused: %s", track->config->id);
            }
            break;

        case PW_STREAM_STATE_STREAMING:
            if (old != PW_STREAM_STATE_STREAMING) {
                track->is_connected = true;
                log_info("Stream started: %s", track->config->id);
                if (track->state != TRACK_STATE_PLAYING) {
                    track->state = TRACK_STATE_PLAYING;
                }
            }
            break;

        case PW_STREAM_STATE_UNCONNECTED:
            track->is_connected = false;
            if (track->state == TRACK_STATE_PLAYING) {
                track->state = TRACK_STATE_DISCONNECTED;
                log_warn("Stream disconnected: %s", track->config->id);
            }
            break;

        default:
            break;
    }
}

static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .process = on_process,
        .state_changed = on_stream_state_changed,
};

// Initialize PipeWire for a track
static bool init_track_pipewire(
        track_manager_ctx_t *ctx,
        track_instance_t *track
) {
    bool success = false; // Used for cleanup handling
    struct pw_properties *props = NULL;
    char channelNames[1024] = "";
    // Build channel names string if mapping is specified
    if (track->config->output.mapping_count > 0) {
        for (int i = 0; i < track->config->output.mapping_count; i++) {
            if (i > 0) {
                if (strlen(channelNames) + 1 >= sizeof(channelNames)) {
                    log_error("Channel names string too long");
                    return false;
                }
                strcat(channelNames, ",");
            }
            if (strlen(channelNames) + strlen(track->config->output.mapping[i]) >= sizeof(channelNames)) {
                log_error("Channel names string too long");
                return false;
            }
            strcat(channelNames, track->config->output.mapping[i]);
        }
    }

    props = pw_properties_new(
            PW_KEY_MEDIA_TYPE,
            "Audio",
            PW_KEY_MEDIA_CATEGORY,
            "Playback",
            PW_KEY_MEDIA_ROLE,
            "Music",
            PW_KEY_NODE_NAME,
            track->config->id,
            PW_KEY_NODE_DESCRIPTION,
            track->config->id,
            NULL
    );

    if (!props) {
        log_error("Failed to create stream properties");
        goto cleanup;
    }

    // Add a device target if specified
    if (track->config->output.device) {
        if (strcmp(track->config->output.device, "default") != 0) {
            if (pw_properties_set(props, PW_KEY_TARGET_OBJECT, track->config->output.device) != 0) {
                log_error("Failed to set target device property");
                goto cleanup;
            }
        }
    }

    // Add channel mapping if specified
    if (track->config->output.mapping_count > 0) {
        pw_properties_set(props, PW_KEY_NODE_CHANNELNAMES, channelNames);
        pw_properties_set(props, PW_KEY_MEDIA_TYPE, "Audio");
        pw_properties_setf(props, PW_KEY_AUDIO_CHANNELS, "%d", track->config->output.mapping_count);
    }

    // Create stream
    track->stream = pw_stream_new_simple(
            pw_main_loop_get_loop(ctx->pw_loop),
            track->config->id,
            props,
            &stream_events,
            track
    );

    if (!track->stream) {
        log_error("Failed to create stream");
        goto cleanup;
    }

    success = true;

    cleanup:
    if (!success && props) {
        pw_properties_free(props);
    }

    if (!success && track->stream) {
        pw_stream_destroy(track->stream);
        track->stream = NULL;
    }

    return success;
}

track_manager_ctx_t *track_manager_init(global_config_t *config) {
    track_manager_ctx_t *ctx = calloc(1, sizeof(track_manager_ctx_t));
    if (!ctx) {
        log_error("Failed to allocate track manager context");
        return NULL;
    }

    ctx->config = config;
    ctx->active_tracks = 0;

    // Initialize PipeWire
    pw_init(NULL, NULL);

    ctx->pw_loop = pw_main_loop_new(NULL);
    if (!ctx->pw_loop) {
        log_error("Failed to create PipeWire main loop");
        free(ctx);
        return NULL;
    }

    ctx->pw_context = pw_context_new(pw_main_loop_get_loop(ctx->pw_loop), NULL, 0);

    if (!ctx->pw_context) {
        log_error("Failed to create PipeWire context");
        pw_main_loop_destroy(ctx->pw_loop);
        free(ctx);
        return NULL;
    }

    ctx->initialized = true;
    return ctx;
}

void track_manager_cleanup(track_manager_ctx_t *ctx) {
    if (!ctx)
        return;

    // Stop all tracks
    track_manager_stop_all(ctx);

    // Cleanup PipeWire
    if (ctx->pw_context)
        pw_context_destroy(ctx->pw_context);
    if (ctx->pw_loop)
        pw_main_loop_destroy(ctx->pw_loop);

    pw_deinit();

    free(ctx);
}

bool track_manager_play(track_manager_ctx_t *ctx, const char *track_id) {
    if (!ctx || !track_id)
        return false;

    // Find track configuration
    track_config_t *config = NULL;
    for (int i = 0; i < ctx->config->track_count; i++) {
        if (strcmp(ctx->config->tracks[i].id, track_id) == 0) {
            config = &ctx->config->tracks[i];
            break;
        }
    }

    if (!config) {
        log_error("Track not found: %s", track_id);
        return false;
    }

    // Check if track is already active
    for (int i = 0; i < ctx->active_tracks; i++) {
        if (strcmp(ctx->tracks[i].config->id, track_id) == 0) {
            track_instance_t *track = &ctx->tracks[i];

            // If track is stopped (finished), restart it
            if (track->state == TRACK_STATE_STOPPED && track->audio_file) {
                audio_file_seek(track->audio_file, 0);
                track->state = TRACK_STATE_PLAYING;
                log_info("Restarting track: %s", track_id);
                return true;
            }

            // Otherwise it's already playing
            log_info("Track already playing: %s", track_id);
            return true;
        }
    }

    // Initialize new track instance
    if (ctx->active_tracks >= MAX_TRACKS) {
        log_error("Maximum number of active tracks reached");
        return false;
    }

    track_instance_t *track = &ctx->tracks[ctx->active_tracks];
    memset(track, 0, sizeof(track_instance_t));
    track->config = config;
    track->state = TRACK_STATE_STOPPED;
    track->is_connected = false;
    track->error.message = NULL;
    track->error.code = 0;

    // Open audio file
    track->audio_file = audio_file_open(config->file_path, config->loop, config->volume);
    if (!track->audio_file) {
        log_error("Failed to open audio file: %s", config->file_path);
        return false;
    }

    // Initialize PipeWire
    if (!init_track_pipewire(ctx, track)) {
        log_error("Failed to initialize PipeWire for track: %s", track_id);
        audio_file_close(track->audio_file);
        return false;
    }

    // Set up stream parameters
    uint8_t buffer[1024];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    struct spa_audio_info_raw audio_info = {
            .format = SPA_AUDIO_FORMAT_F32,
            .channels = track->config->output.mapping_count > 0 ? track->config->output.mapping_count
                                                                : track->audio_file->info.channels,
            .rate = track->audio_file->info.samplerate
    };

    // Set channel positions
    if (track->config->output.mapping_count > 0) {
        // Set default mapping first
        for (uint8_t i = 0; i < SPA_AUDIO_MAX_CHANNELS; i++) {
            audio_info.position[i] = SPA_AUDIO_CHANNEL_UNKNOWN;
        }

        // Map each channel according to configuration
        for (uint8_t i = 0; i < track->config->output.mapping_count && i < SPA_AUDIO_MAX_CHANNELS; i++) {
            const char *port_name = track->config->output.mapping[i];
            audio_info.position[i] = get_channel_position(port_name);
            if (audio_info.position[i] == SPA_AUDIO_CHANNEL_UNKNOWN) {
                log_warn("Unknown channel name '%s', using UNKNOWN", port_name);
            }
        }
        audio_info.channels = track->config->output.mapping_count;
    } else {
        // If no mapping specified, use sequential AUX channels
        audio_info.channels = track->audio_file->info.channels;
        for (uint8_t i = 0; i < audio_info.channels && i < SPA_AUDIO_MAX_CHANNELS; i++) {
            audio_info.position[i] = SPA_AUDIO_CHANNEL_AUX0 + i;
        }
    }

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    if (pw_stream_connect(
            track->stream,
            PW_DIRECTION_OUTPUT,
            PW_ID_ANY,
            PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
            params,
            1
    ) < 0) {
        log_error("Failed to connect stream");
        pw_stream_destroy(track->stream);
        audio_file_close(track->audio_file);
        return false;
    }

    track->state = TRACK_STATE_PLAYING;
    ctx->active_tracks++;
    log_info("Started playback of track: %s", track_id);

    return true;
}

bool track_manager_stop(track_manager_ctx_t *ctx, const char *track_id) {
    if (!ctx || !track_id)
        return false;

    for (int i = 0; i < ctx->active_tracks; i++) {
        if (strcmp(ctx->tracks[i].config->id, track_id) == 0) {
            track_instance_t *track = &ctx->tracks[i];

            // Clean up error message if any
            if (track->error.message) {
                free(track->error.message);
                track->error.message = NULL;
            }

            // Destroy PipeWire stream
            if (track->stream) {
                pw_stream_destroy(track->stream);
                track->stream = NULL;
            }

            // Close audio file if it exists
            if (track->audio_file) {
                audio_file_close(track->audio_file);
                track->audio_file = NULL;
            }

            // Remove track from active tracks
            if (i < ctx->active_tracks - 1) {
                memmove(
                        &ctx->tracks[i],
                        &ctx->tracks[i + 1],
                        sizeof(track_instance_t) * (ctx->active_tracks - i - 1)
                );
            }
            ctx->active_tracks--;

            log_info("Stopped track: %s", track_id);
            return true;
        }
    }

    log_warn("Track not playing: %s", track_id);
    return false;
}

bool track_manager_stop_all(track_manager_ctx_t *ctx) {
    if (!ctx)
        return false;

    while (ctx->active_tracks > 0) {
        track_manager_stop(ctx, ctx->tracks[0].config->id);
    }

    return true;
}

bool track_manager_is_playing(track_manager_ctx_t *ctx, const char *track_id) {
    if (!ctx || !track_id)
        return false;

    for (int i = 0; i < ctx->active_tracks; i++) {
        if (strcmp(ctx->tracks[i].config->id, track_id) == 0) {
            return ctx->tracks[i].state == TRACK_STATE_PLAYING;
        }
    }

    return false;
}

void track_manager_list_tracks(track_manager_ctx_t *ctx) {
    if (!ctx)
        return;

    printf("Configured tracks:\n");
    for (int i = 0; i < ctx->config->track_count; i++) {
        track_config_t *track = &ctx->config->tracks[i];
        bool is_playing = track_manager_is_playing(ctx, track->id);

        printf("  %s:\n", track->id);
        printf("    File: %s\n", track->file_path);
        printf("    Loop: %s\n", track->loop ? "yes" : "no");
        printf("    Volume: %.2f\n", track->volume);
        printf("    Status: %s\n", is_playing ? "playing" : "stopped");
    }
}

char *track_manager_print_status(track_manager_ctx_t *ctx) {
    char *status = malloc(4096); // Generous buffer size
    int offset = 0;

    if (!ctx) {
        snprintf(status, 4096, "Track manager not initialized\n");
        return status;
    }

    offset += snprintf(status + offset, 4096 - offset, "Active tracks: %d\n", ctx->active_tracks);

    for (int i = 0; i < ctx->active_tracks; i++) {
        const char *state_str;
        track_instance_t *track = &ctx->tracks[i];
        switch (ctx->tracks[i].state) {
            case TRACK_STATE_PLAYING:
                state_str = "playing";
                break;
            case TRACK_STATE_STOPPED:
                state_str = "stopped";
                break;
            case TRACK_STATE_ERROR:
                state_str = track->error.message ? track->error.message : "error";
                break;
            case TRACK_STATE_CONNECTING:
                state_str = "connecting";
                break;
            case TRACK_STATE_DISCONNECTED:
                state_str = "disconnected";
                break;
            default:
                state_str = "unknown";
                break;
        }

        offset += snprintf(
            status + offset,
            4096 - offset,
            "Track %s: %s (connected: %s)\n",
            ctx->tracks[i].config->id,
            state_str,
            ctx->tracks[i].is_connected ? "yes" : "no"
        );

        if (ctx->tracks[i].state == TRACK_STATE_ERROR && ctx->tracks[i].error.message) {
            offset += snprintf(status + offset, 4096 - offset, "  Error: %s\n", ctx->tracks[i].error.message);
        }
    }

    return status;
}

// Test tone configuration
static track_config_t TEST_TONE_CONFIG = {
        .id = "test_tone",
        .loop = true,
        .volume = 0.5f,
        .output = {
                .device = "default",
                .mapping = NULL,
                .mapping_count = 0
        }
};

// Helper function to parse channel mapping string
static bool parse_channel_mapping(const char *mapping_str, track_config_t *config) {
    if (!mapping_str || !config) return false;

    // Count commas to determine number of channels
    int count = 1;
    for (const char *p = mapping_str; *p; p++) {
        if (*p == ',') count++;
    }

    // Allocate space for channel names
    char **mappings = calloc(count, sizeof(char *));
    if (!mappings) return false;

    // Copy the string so we can modify it
    char *str = strdup(mapping_str);
    if (!str) {
        free(mappings);
        return false;
    }

    // Parse the channels
    char *token = strtok(str, ",");
    int i = 0;
    while (token && i < count) {
        mappings[i] = strdup(token);
        if (!mappings[i]) {
            // Cleanup on error
            for (int j = 0; j < i; j++) {
                free(mappings[j]);
            }
            free(mappings);
            free(str);
            return false;
        }
        i++;
        token = strtok(NULL, ",");
    }

    // Update config
    config->output.mapping = mappings;
    config->output.mapping_count = count;

    free(str);
    return true;
}

static void generate_test_tone(
        float *buffer,
        size_t frames,
        int channels,
        int sample_rate
) {
    static float phase = 0.0f;
    const float frequency = 440.0f; // A4 note
    const float increment = 2.0f * M_PI * frequency / sample_rate;

    for (size_t i = 0; i < frames; i++) {
        float sample = sinf(phase) * 0.5f; // 50% amplitude
        for (int ch = 0; ch < channels; ch++) {
            buffer[i * channels + ch] = sample;
        }
        phase += increment;
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }
    }
}

static void on_test_tone_process(void *userdata) {
    track_instance_t *track = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *dst;

    if ((b = pw_stream_dequeue_buffer(track->stream)) == NULL) {
        log_error("Out of buffers");
        return;
    }

    buf = b->buffer;
    dst = buf->datas[0].data;
    if (dst == NULL)
        return;

    size_t n_frames = buf->datas[0].maxsize / sizeof(float) /
                      track->config->output.mapping_count;

    // Generate test tone
    generate_test_tone(dst, n_frames, track->config->output.mapping_count, 48000);

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = track->config->output.mapping_count * sizeof(float);
    buf->datas[0].chunk->size = n_frames * track->config->output.mapping_count * sizeof(float);

    pw_stream_queue_buffer(track->stream, b);
}

bool track_manager_play_test_tone(track_manager_ctx_t *ctx, const char *channel_mapping) {
    // Reset test tone configuration
    TEST_TONE_CONFIG.output.mapping = NULL;
    TEST_TONE_CONFIG.output.mapping_count = 0;

    // If channel mapping is provided, parse it
    if (channel_mapping) {
        if (!parse_channel_mapping(channel_mapping, &TEST_TONE_CONFIG)) {
            log_error("Failed to parse channel mapping: %s", channel_mapping);
            return false;
        }
    } else {
        // Default mapping if none provided
        static char *default_mapping[] = {"FL", "FR"};
        TEST_TONE_CONFIG.output.mapping = default_mapping;
        TEST_TONE_CONFIG.output.mapping_count = 2;
    }

    if (!ctx)
        return false;

    // Stop any existing test tone
    track_manager_stop(ctx, TEST_TONE_CONFIG.id);

    // Initialize new track instance
    if (ctx->active_tracks >= MAX_TRACKS) {
        log_error("Maximum number of active tracks reached");
        return false;
    }

    track_instance_t *track = &ctx->tracks[ctx->active_tracks];
    memset(track, 0, sizeof(track_instance_t));
    track->config = (track_config_t *) &TEST_TONE_CONFIG;
    track->state = TRACK_STATE_STOPPED;

    // Set up PipeWire stream
    struct pw_properties *props = pw_properties_new(
            PW_KEY_MEDIA_TYPE,
            "Audio",
            PW_KEY_MEDIA_CATEGORY,
            "Playback",
            PW_KEY_MEDIA_ROLE,
            "Test",
            PW_KEY_NODE_NAME,
            "test_tone",
            PW_KEY_NODE_DESCRIPTION,
            "Test Tone Generator",
            NULL
    );

    if (!props) {
        log_error("Failed to create stream properties");
        return false;
    }

    // Set up channel mapping
    char channelNames[1024] = "";
    for (int i = 0; i < track->config->output.mapping_count; i++) {
        if (i > 0)
            strcat(channelNames, ",");
        strcat(channelNames, track->config->output.mapping[i]);
    }
    pw_properties_set(props, PW_KEY_NODE_CHANNELNAMES, channelNames);

    // Create stream with test tone process callback
    struct pw_stream_events test_events = stream_events;
    test_events.process = on_test_tone_process;

    track->stream = pw_stream_new_simple(
            pw_main_loop_get_loop(ctx->pw_loop),
            "test_tone",
            props,
            &test_events,
            track
    );

    if (!track->stream) {
        log_error("Failed to create test tone stream");
        pw_properties_free(props);
        return false;
    }

    // Set up stream parameters
    uint8_t buffer[1024];
    struct spa_pod_builder b;
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    struct spa_audio_info_raw audio_info = {
            .format = SPA_AUDIO_FORMAT_F32,
            .channels = track->config->output.mapping_count,
            .rate = 48000
    };

    // Set channel positions
    for (uint8_t i = 0; i < audio_info.channels && i < SPA_AUDIO_MAX_CHANNELS;
         i++) {
        audio_info.position[i] = SPA_AUDIO_CHANNEL_AUX0 + i;
    }

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    if (pw_stream_connect(
            track->stream,
            PW_DIRECTION_OUTPUT,
            PW_ID_ANY,
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS |
            PW_STREAM_FLAG_RT_PROCESS,
            params,
            1
    ) < 0) {
        log_error("Failed to connect test tone stream");
        pw_stream_destroy(track->stream);
        pw_properties_free(props);
        return false;
    }

    track->state = TRACK_STATE_PLAYING;
    ctx->active_tracks++;
    log_info("Started test tone playback");

    pw_properties_free(props);
    return true;
}

void track_manager_process_events(track_manager_ctx_t *ctx) {
    if (!ctx || !ctx->pw_loop)
        return;

    // Iterate the PipeWire main loop once with no timeout (non-blocking)
    pw_loop_iterate(pw_main_loop_get_loop(ctx->pw_loop), 0);
}
