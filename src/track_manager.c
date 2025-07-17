#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include "track_manager.h"
#include "log.h"

#define MAX_TRACKS 32
#define BUFFER_SIZE 4096

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

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
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
    return true;
}

bool track_manager_stop(track_manager_ctx_t *ctx, const char *track_id) {
    if (!ctx || !track_id) return false;

    for (int i = 0; i < ctx->active_tracks; i++) {
        if (strcmp(ctx->tracks[i].config->id, track_id) == 0) {
            // TODO: Stop playback thread
            // TODO: Cleanup resources

            // Remove track from active tracks
            if (i < ctx->active_tracks - 1) {
                memmove(&ctx->tracks[i], &ctx->tracks[i + 1], 
                        sizeof(track_instance_t) * (ctx->active_tracks - i - 1));
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
        printf("  %s: %s\n", track->config->id,
               track->state == TRACK_STATE_PLAYING ? "playing" :
               track->state == TRACK_STATE_STOPPED ? "stopped" : "error");
    }
}

bool track_manager_play_test_tone(track_manager_ctx_t *ctx) {
    if (!ctx) return false;

    // TODO: Implement test tone
    log_info("Test tone not implemented yet");
    return false;
}
