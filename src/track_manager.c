#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include "track_manager.h"
#include "log.h"
#include "mqtt_client.h"

#define MAX_TRACKS 32
#define BUFFER_SIZE 4096

// Build channel name from enum value
static const char* get_channel_name_from_enum(enum spa_audio_channel position) {
    switch (position) {
        case SPA_AUDIO_CHANNEL_UNKNOWN: return "UNKNOWN";
        case SPA_AUDIO_CHANNEL_NA: return "NA";
        case SPA_AUDIO_CHANNEL_MONO: return "MONO";
        case SPA_AUDIO_CHANNEL_FL: return "FL";
        case SPA_AUDIO_CHANNEL_FR: return "FR";
        case SPA_AUDIO_CHANNEL_FC: return "FC";
        case SPA_AUDIO_CHANNEL_LFE: return "LFE";
        case SPA_AUDIO_CHANNEL_SL: return "SL";
        case SPA_AUDIO_CHANNEL_SR: return "SR";
        case SPA_AUDIO_CHANNEL_FLC: return "FLC";
        case SPA_AUDIO_CHANNEL_FRC: return "FRC";
        case SPA_AUDIO_CHANNEL_RC: return "RC";
        case SPA_AUDIO_CHANNEL_RL: return "RL";
        case SPA_AUDIO_CHANNEL_RR: return "RR";
        case SPA_AUDIO_CHANNEL_TC: return "TC";
        case SPA_AUDIO_CHANNEL_TFL: return "TFL";
        case SPA_AUDIO_CHANNEL_TFC: return "TFC";
        case SPA_AUDIO_CHANNEL_TFR: return "TFR";
        case SPA_AUDIO_CHANNEL_TRL: return "TRL";
        case SPA_AUDIO_CHANNEL_TRC: return "TRC";
        case SPA_AUDIO_CHANNEL_TRR: return "TRR";
        case SPA_AUDIO_CHANNEL_RLC: return "RLC";
        case SPA_AUDIO_CHANNEL_RRC: return "RRC";
        case SPA_AUDIO_CHANNEL_FLW: return "FLW";
        case SPA_AUDIO_CHANNEL_FRW: return "FRW";
        case SPA_AUDIO_CHANNEL_LFE2: return "LFE2";
        case SPA_AUDIO_CHANNEL_FLH: return "FLH";
        case SPA_AUDIO_CHANNEL_FCH: return "FCH";
        case SPA_AUDIO_CHANNEL_FRH: return "FRH";
        case SPA_AUDIO_CHANNEL_TFLC: return "TFLC";
        case SPA_AUDIO_CHANNEL_TFRC: return "TFRC";
        case SPA_AUDIO_CHANNEL_TSL: return "TSL";
        case SPA_AUDIO_CHANNEL_TSR: return "TSR";
        case SPA_AUDIO_CHANNEL_LLFE: return "LLFE";
        case SPA_AUDIO_CHANNEL_RLFE: return "RLFE";
        case SPA_AUDIO_CHANNEL_BC: return "BC";
        case SPA_AUDIO_CHANNEL_BLC: return "BLC";
        case SPA_AUDIO_CHANNEL_BRC: return "BRC";
        default:
            if (position >= SPA_AUDIO_CHANNEL_START_Aux && 
                position <= SPA_AUDIO_CHANNEL_LAST_Aux) {
                static char aux_name[8];
                snprintf(aux_name, sizeof(aux_name), "AUX%d", 
                        position - SPA_AUDIO_CHANNEL_START_Aux);
                return aux_name;
            }
            return "UNKNOWN";
    }
}

// Get enum value from channel name
static enum spa_audio_channel get_channel_position(const char *name) {
    if (!name) return SPA_AUDIO_CHANNEL_UNKNOWN;

    // Check standard channels first
    if (strcmp(name, "UNKNOWN") == 0) return SPA_AUDIO_CHANNEL_UNKNOWN;
    if (strcmp(name, "NA") == 0) return SPA_AUDIO_CHANNEL_NA;
    if (strcmp(name, "MONO") == 0) return SPA_AUDIO_CHANNEL_MONO;
    if (strcmp(name, "FL") == 0) return SPA_AUDIO_CHANNEL_FL;
    if (strcmp(name, "FR") == 0) return SPA_AUDIO_CHANNEL_FR;
    if (strcmp(name, "FC") == 0) return SPA_AUDIO_CHANNEL_FC;
    if (strcmp(name, "LFE") == 0) return SPA_AUDIO_CHANNEL_LFE;
    if (strcmp(name, "SL") == 0) return SPA_AUDIO_CHANNEL_SL;
    if (strcmp(name, "SR") == 0) return SPA_AUDIO_CHANNEL_SR;
    if (strcmp(name, "FLC") == 0) return SPA_AUDIO_CHANNEL_FLC;
    if (strcmp(name, "FRC") == 0) return SPA_AUDIO_CHANNEL_FRC;
    if (strcmp(name, "RC") == 0) return SPA_AUDIO_CHANNEL_RC;
    if (strcmp(name, "RL") == 0) return SPA_AUDIO_CHANNEL_RL;
    if (strcmp(name, "RR") == 0) return SPA_AUDIO_CHANNEL_RR;
    if (strcmp(name, "TC") == 0) return SPA_AUDIO_CHANNEL_TC;
    if (strcmp(name, "TFL") == 0) return SPA_AUDIO_CHANNEL_TFL;
    if (strcmp(name, "TFC") == 0) return SPA_AUDIO_CHANNEL_TFC;
    if (strcmp(name, "TFR") == 0) return SPA_AUDIO_CHANNEL_TFR;
    if (strcmp(name, "TRL") == 0) return SPA_AUDIO_CHANNEL_TRL;
    if (strcmp(name, "TRC") == 0) return SPA_AUDIO_CHANNEL_TRC;
    if (strcmp(name, "TRR") == 0) return SPA_AUDIO_CHANNEL_TRR;
    if (strcmp(name, "RLC") == 0) return SPA_AUDIO_CHANNEL_RLC;
    if (strcmp(name, "RRC") == 0) return SPA_AUDIO_CHANNEL_RRC;
    if (strcmp(name, "FLW") == 0) return SPA_AUDIO_CHANNEL_FLW;
    if (strcmp(name, "FRW") == 0) return SPA_AUDIO_CHANNEL_FRW;
    if (strcmp(name, "LFE2") == 0) return SPA_AUDIO_CHANNEL_LFE2;
    if (strcmp(name, "FLH") == 0) return SPA_AUDIO_CHANNEL_FLH;
    if (strcmp(name, "FCH") == 0) return SPA_AUDIO_CHANNEL_FCH;
    if (strcmp(name, "FRH") == 0) return SPA_AUDIO_CHANNEL_FRH;
    if (strcmp(name, "TFLC") == 0) return SPA_AUDIO_CHANNEL_TFLC;
    if (strcmp(name, "TFRC") == 0) return SPA_AUDIO_CHANNEL_TFRC;
    if (strcmp(name, "TSL") == 0) return SPA_AUDIO_CHANNEL_TSL;
    if (strcmp(name, "TSR") == 0) return SPA_AUDIO_CHANNEL_TSR;
    if (strcmp(name, "LLFE") == 0) return SPA_AUDIO_CHANNEL_LLFE;
    if (strcmp(name, "RLFE") == 0) return SPA_AUDIO_CHANNEL_RLFE;
    if (strcmp(name, "BC") == 0) return SPA_AUDIO_CHANNEL_BC;
    if (strcmp(name, "BLC") == 0) return SPA_AUDIO_CHANNEL_BLC;
    if (strcmp(name, "BRC") == 0) return SPA_AUDIO_CHANNEL_BRC;

    // Check for AUX channels
    if (strncmp(name, "AUX", 3) == 0) {
        int aux_num = atoi(name + 3);
        if (aux_num >= 0 && aux_num <= (SPA_AUDIO_CHANNEL_LAST_Aux - SPA_AUDIO_CHANNEL_START_Aux)) {
            return SPA_AUDIO_CHANNEL_START_Aux + aux_num;
        }
    }

    return SPA_AUDIO_CHANNEL_UNKNOWN;
}
    {"UNKNOWN", SPA_AUDIO_CHANNEL_UNKNOWN},
    {"NA",      SPA_AUDIO_CHANNEL_NA},
    {"MONO",    SPA_AUDIO_CHANNEL_MONO},
    // Front
    {"FL",      SPA_AUDIO_CHANNEL_FL},   // Front Left
    {"FR",      SPA_AUDIO_CHANNEL_FR},   // Front Right
    {"FC",      SPA_AUDIO_CHANNEL_FC},   // Front Center
    {"LFE",     SPA_AUDIO_CHANNEL_LFE},  // Low Frequency Effects
    {"SL",      SPA_AUDIO_CHANNEL_SL},   // Side Left
    {"SR",      SPA_AUDIO_CHANNEL_SR},   // Side Right
    {"FLC",     SPA_AUDIO_CHANNEL_FLC},  // Front Left Center
    {"FRC",     SPA_AUDIO_CHANNEL_FRC},  // Front Right Center
    {"RC",      SPA_AUDIO_CHANNEL_RC},   // Rear Center
    {"RL",      SPA_AUDIO_CHANNEL_RL},   // Rear Left
    {"RR",      SPA_AUDIO_CHANNEL_RR},   // Rear Right
    // Top
    {"TC",      SPA_AUDIO_CHANNEL_TC},   // Top Center
    {"TFL",     SPA_AUDIO_CHANNEL_TFL},  // Top Front Left
    {"TFC",     SPA_AUDIO_CHANNEL_TFC},  // Top Front Center
    {"TFR",     SPA_AUDIO_CHANNEL_TFR},  // Top Front Right
    {"TRL",     SPA_AUDIO_CHANNEL_TRL},  // Top Rear Left
    {"TRC",     SPA_AUDIO_CHANNEL_TRC},  // Top Rear Center
    {"TRR",     SPA_AUDIO_CHANNEL_TRR},  // Top Rear Right
    // Additional channels
    {"RLC",     SPA_AUDIO_CHANNEL_RLC},  // Rear Left Center
    {"RRC",     SPA_AUDIO_CHANNEL_RRC},  // Rear Right Center
    {"FLW",     SPA_AUDIO_CHANNEL_FLW},  // Front Left Wide
    {"FRW",     SPA_AUDIO_CHANNEL_FRW},  // Front Right Wide
    {"LFE2",    SPA_AUDIO_CHANNEL_LFE2}, // Second Low Frequency Effects
    {"FLH",     SPA_AUDIO_CHANNEL_FLH},  // Front Left High
    {"FCH",     SPA_AUDIO_CHANNEL_FCH},  // Front Center High
    {"FRH",     SPA_AUDIO_CHANNEL_FRH},  // Front Right High
    {"TFLC",    SPA_AUDIO_CHANNEL_TFLC}, // Top Front Left Center
    {"TFRC",    SPA_AUDIO_CHANNEL_TFRC}, // Top Front Right Center
    {"TSL",     SPA_AUDIO_CHANNEL_TSL},  // Top Side Left
    {"TSR",     SPA_AUDIO_CHANNEL_TSR},  // Top Side Right
    {"LLFE",    SPA_AUDIO_CHANNEL_LLFE}, // Left LFE
    {"RLFE",    SPA_AUDIO_CHANNEL_RLFE}, // Right LFE
    {"BC",      SPA_AUDIO_CHANNEL_BC},   // Bottom Center
    {"BLC",     SPA_AUDIO_CHANNEL_BLC},  // Bottom Left Center
    {"BRC",     SPA_AUDIO_CHANNEL_BRC},  // Bottom Right Center
    // AUX channels (0-63)
    {"AUX0",    SPA_AUDIO_CHANNEL_AUX0},
    {"AUX1",    SPA_AUDIO_CHANNEL_AUX1},
    {"AUX2",    SPA_AUDIO_CHANNEL_AUX2},
    {"AUX3",    SPA_AUDIO_CHANNEL_AUX3},
    {"AUX4",    SPA_AUDIO_CHANNEL_AUX4},
    {"AUX5",    SPA_AUDIO_CHANNEL_AUX5},
    {"AUX6",    SPA_AUDIO_CHANNEL_AUX6},
    {"AUX7",    SPA_AUDIO_CHANNEL_AUX7},
    {"AUX8",    SPA_AUDIO_CHANNEL_AUX8},
    {"AUX9",    SPA_AUDIO_CHANNEL_AUX9},
    {"AUX10",   SPA_AUDIO_CHANNEL_AUX10},
    {"AUX11",   SPA_AUDIO_CHANNEL_AUX11},
    {"AUX12",   SPA_AUDIO_CHANNEL_AUX12},
    {"AUX13",   SPA_AUDIO_CHANNEL_AUX13},
    {"AUX14",   SPA_AUDIO_CHANNEL_AUX14},
    {"AUX15",   SPA_AUDIO_CHANNEL_AUX15},
    // ... Add more AUX channels as needed up to AUX63
    {NULL, 0} // Terminator
};

// Get channel position from name
static uint32_t get_channel_position(const char *name) {
    if (!name) return SPA_AUDIO_CHANNEL_UNKNOWN;

    for (const channel_map_t *map = CHANNEL_MAP; map->name != NULL; map++) {
        if (strcasecmp(map->name, name) == 0) {
            return map->position;
        }
    }

    return SPA_AUDIO_CHANNEL_UNKNOWN;
}

// Get channel name from position
static const char* get_channel_name(uint32_t position) {
    for (const channel_map_t *map = CHANNEL_MAP; map->name != NULL; map++) {
        if (map->position == position) {
            return map->name;
        }
    }

    return "UNKNOWN";
}

struct track_manager_ctx {
    global_config_t *config;
    track_instance_t tracks[MAX_TRACKS];
    int active_tracks;
    struct pw_context *pw_context;
    struct pw_main_loop *pw_loop;
    bool initialized;
};

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

    size_t n_frames = buf->datas[0].maxsize / sizeof(float) / track->audio_file->info.channels;

    // Read audio data
    size_t frames_read = audio_file_read(track->audio_file, dst, n_frames);

    if (frames_read < n_frames) {
        if (!track->audio_file->loop) {
            // End of file reached and not looping
            track->state = TRACK_STATE_STOPPED;
            log_info("Track finished: %s", track->config->id);
        }
        // Fill remaining buffer with silence
        memset(dst + (frames_read * track->audio_file->info.channels), 0,
               (n_frames - frames_read) * track->audio_file->info.channels * sizeof(float));
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = track->audio_file->info.channels * sizeof(float);
    buf->datas[0].chunk->size = n_frames * track->audio_file->info.channels * sizeof(float);

    pw_stream_queue_buffer(track->stream, b);
}

static void on_stream_state_changed(void *userdata, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
    track_instance_t *track = userdata;

    log_debug("Stream state changed from %s to %s", 
              pw_stream_state_as_string(old),
              pw_stream_state_as_string(state));

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
static bool init_track_pipewire(track_manager_ctx_t *ctx, track_instance_t *track) {
    struct pw_properties *props;

    char channelnames[1024] = "";
    if (track->config->output.mapping_count > 0) {
        for (int i = 0; i < track->config->output.mapping_count; i++) {
            if (i > 0) strcat(channelnames, ",");
            strcat(channelnames, track->config->output.mapping[i]);
        }
    }

    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_NODE_NAME, track->config->id,
        PW_KEY_NODE_DESCRIPTION, track->config->id,
        NULL);

    if (!props) {
        log_error("Failed to create stream properties");
        return false;
    }

    // Add device target if specified
    if (track->config->output.device) {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, track->config->output.device);
    }

    // Add channel mapping if specified
    if (track->config->output.mapping_count > 0) {
        pw_properties_set(props, PW_KEY_NODE_CHANNELNAMES, channelnames);
        pw_properties_set(props, PW_KEY_MEDIA_TYPE, "Audio");
        pw_properties_setf(props, PW_KEY_AUDIO_CHANNELS, "%d", track->config->output.mapping_count);
    }

    track->stream = pw_stream_new_simple(
        pw_main_loop_get_loop(ctx->pw_loop),
        track->config->id,
        props,
        &stream_events,
        track);

    if (!track->stream) {
        log_error("Failed to create stream");
        pw_properties_free(props);
        return false;
    }

    pw_properties_free(props);
    return true;
}

track_manager_ctx_t* track_manager_init(global_config_t *config) {
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

    ctx->pw_context = pw_context_new(
        pw_main_loop_get_loop(ctx->pw_loop),
        NULL,
        0);

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
    if (!ctx) return;

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
    if (!ctx || !track_id) return false;

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

    // Check if track is already playing
    for (int i = 0; i < ctx->active_tracks; i++) {
        if (strcmp(ctx->tracks[i].config->id, track_id) == 0) {
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
        .channels = track->config->output.mapping_count > 0 ? 
                   track->config->output.mapping_count : 
                   track->audio_file->info.channels,
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

    if (pw_stream_connect(track->stream,
                         PW_DIRECTION_OUTPUT,
                         PW_ID_ANY,
                         PW_STREAM_FLAG_AUTOCONNECT |
                         PW_STREAM_FLAG_MAP_BUFFERS |
                         PW_STREAM_FLAG_RT_PROCESS,
                         params, 1) < 0) {
        log_error("Failed to connect stream");
        pw_stream_destroy(track->stream);
        audio_file_close(track->audio_file);
        return false;
    }

    track->state = TRACK_STATE_PLAYING;
    ctx->active_tracks++;
    log_info("Started playback of track: %s", track_id);

    // Publish MQTT status if enabled
    if (ctx->config->mqtt_ctx) {
        mqtt_client_publish_status(ctx->config->mqtt_ctx, track_id, true);
    }

    return true;
}

bool track_manager_stop(track_manager_ctx_t *ctx, const char *track_id) {
    if (!ctx || !track_id) return false;

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
                memmove(&ctx->tracks[i], &ctx->tracks[i + 1], 
                        sizeof(track_instance_t) * (ctx->active_tracks - i - 1));
            }
            ctx->active_tracks--;

            // Publish MQTT status if enabled
            if (ctx->config->mqtt_ctx) {
                mqtt_client_publish_status(ctx->config->mqtt_ctx, track_id, false);
            }

            log_info("Stopped track: %s", track_id);
            return true;
        }
    }

    log_warn("Track not playing: %s", track_id);
    return false;
}

bool track_manager_stop_all(track_manager_ctx_t *ctx) {
    if (!ctx) return false;

    while (ctx->active_tracks > 0) {
        track_manager_stop(ctx, ctx->tracks[0].config->id);
    }

    return true;
}

bool track_manager_is_playing(track_manager_ctx_t *ctx, const char *track_id) {
    if (!ctx || !track_id) return false;

    for (int i = 0; i < ctx->active_tracks; i++) {
        if (strcmp(ctx->tracks[i].config->id, track_id) == 0) {
            return ctx->tracks[i].state == TRACK_STATE_PLAYING;
        }
    }

    return false;
}

void track_manager_list_tracks(track_manager_ctx_t *ctx) {
    if (!ctx) return;

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

void track_manager_print_status(track_manager_ctx_t *ctx) {
    if (!ctx) return;

    printf("Active tracks: %d/%d\n", ctx->active_tracks, MAX_TRACKS);
    for (int i = 0; i < ctx->active_tracks; i++) {
        track_instance_t *track = &ctx->tracks[i];
        const char *state_str;
        switch (track->state) {
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
        printf("  %s: %s\n", track->config->id, state_str);
        if (track->config->output.device) {
            printf("    Device: %s\n", track->config->output.device);
        }
        printf("    Connected: %s\n", track->is_connected ? "yes" : "no");
    }
}

// Test tone configuration
static const track_config_t TEST_TONE_CONFIG = {
    .id = "test_tone",
    .loop = true,
    .volume = 0.5f,
    .output = {
        .device = "default",
        .mapping = (char *[]){"AUX0", "AUX1"},
        .mapping_count = 2
    }
};

static void generate_test_tone(float *buffer, size_t frames, int channels, int sample_rate) {
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

    size_t n_frames = buf->datas[0].maxsize / sizeof(float) / track->config->output.mapping_count;

    // Generate test tone
    generate_test_tone(dst, n_frames, track->config->output.mapping_count, 48000);

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = track->config->output.mapping_count * sizeof(float);
    buf->datas[0].chunk->size = n_frames * track->config->output.mapping_count * sizeof(float);

    pw_stream_queue_buffer(track->stream, b);
}

bool track_manager_play_test_tone(track_manager_ctx_t *ctx) {
    if (!ctx) return false;

    // Stop any existing test tone
    track_manager_stop(ctx, TEST_TONE_CONFIG.id);

    // Initialize new track instance
    if (ctx->active_tracks >= MAX_TRACKS) {
        log_error("Maximum number of active tracks reached");
        return false;
    }

    track_instance_t *track = &ctx->tracks[ctx->active_tracks];
    memset(track, 0, sizeof(track_instance_t));
    track->config = (track_config_t *)&TEST_TONE_CONFIG;
    track->state = TRACK_STATE_STOPPED;

    // Set up PipeWire stream
    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Test",
        PW_KEY_NODE_NAME, "test_tone",
        PW_KEY_NODE_DESCRIPTION, "Test Tone Generator",
        NULL);

    if (!props) {
        log_error("Failed to create stream properties");
        return false;
    }

    // Set up channel mapping
    char channelnames[1024] = "";
    for (int i = 0; i < track->config->output.mapping_count; i++) {
        if (i > 0) strcat(channelnames, ",");
        strcat(channelnames, track->config->output.mapping[i]);
    }
    pw_properties_set(props, PW_KEY_NODE_CHANNELNAMES, channelnames);

    // Create stream with test tone process callback
    struct pw_stream_events test_events = stream_events;
    test_events.process = on_test_tone_process;

    track->stream = pw_stream_new_simple(
        pw_main_loop_get_loop(ctx->pw_loop),
        "test_tone",
        props,
        &test_events,
        track);

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
    for (uint8_t i = 0; i < audio_info.channels && i < SPA_AUDIO_MAX_CHANNELS; i++) {
        audio_info.position[i] = SPA_AUDIO_CHANNEL_AUX0 + i;
    }

    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    if (pw_stream_connect(track->stream,
                         PW_DIRECTION_OUTPUT,
                         PW_ID_ANY,
                         PW_STREAM_FLAG_AUTOCONNECT |
                         PW_STREAM_FLAG_MAP_BUFFERS |
                         PW_STREAM_FLAG_RT_PROCESS,
                         params, 1) < 0) {
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
