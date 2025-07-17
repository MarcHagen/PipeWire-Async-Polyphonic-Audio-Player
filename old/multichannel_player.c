/* Multichannel Pipewire Audio Player
 * Capable of playing 12 channels of audio with custom channel mapping
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __has_include
  #if __has_include(<spa-0.2/spa/param/audio/format-utils.h>)
    #include <spa-0.2/spa/param/audio/format-utils.h>
    #include <spa-0.2/spa/param/props.h>
    #include <spa-0.2/spa/utils/string.h>
  #endif
#else
  #include <spa/param/audio/format-utils.h>
  #include <spa/param/props.h>
  #include <spa/utils/string.h>
#endif

#ifdef __has_include
  #if __has_include(<pipewire-0.3/pipewire/pipewire.h>)
    #include <pipewire-0.3/pipewire/pipewire.h>
  #endif
#else
  #include <pipewire/pipewire.h>
#endif

#define CHANNELS    12
#define RATE        48000
#define BUFFER_SIZE 4096

struct data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_audio_info_raw info;

    float *buffer;
    size_t buffer_size;

    char *filename;
    FILE *file;
    bool file_loaded;
};

static void on_process(void *userdata);
static const struct pw_stream_events stream_events;
static void signal_handler(int sig);

static struct data data = { 0 };

static void on_process(void *userdata)
{
    struct data *d = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *dst;
    uint32_t n_frames, n_channels, stride;
    size_t bytes_read;

    if ((b = pw_stream_dequeue_buffer(d->stream)) == NULL) {
        fprintf(stderr, "out of buffers\n");
        return;
    }

    buf = b->buffer;
    dst = buf->datas[0].data;
    if (dst == NULL)
        return;

    n_frames = buf->datas[0].maxsize / sizeof(float) / d->info.channels;
    n_channels = d->info.channels;
    stride = n_channels * sizeof(float);

    if (d->file_loaded) {
        bytes_read = fread(dst, 1, n_frames * stride, d->file);
        if (bytes_read < n_frames * stride) {
            // End of file or error, fill rest with silence
            memset((uint8_t*)dst + bytes_read, 0, n_frames * stride - bytes_read);

            // Loop back to beginning
            if (feof(d->file) && !ferror(d->file)) {
                fseek(d->file, 0, SEEK_SET);
            }
        }
    } else {
        // Generate test tone if no file is loaded
        float *samples = dst;
        static float accumulator = 0.0f;
        float increment = 2.0f * M_PI * 440.0f / RATE; // 440Hz tone

        for (uint32_t i = 0; i < n_frames; i++) {
            float sample = sinf(accumulator) * 0.5f;

            for (uint32_t c = 0; c < n_channels; c++) {
                // Different amplitude for each channel for testing
                samples[i * n_channels + c] = sample * (1.0f - (float)c / n_channels);
            }

            accumulator += increment;
            if (accumulator >= 2.0f * M_PI)
                accumulator -= 2.0f * M_PI;
        }
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = n_frames * stride;

    pw_stream_queue_buffer(d->stream, b);
}

static void on_stream_state_changed(void *userdata, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
    struct data *d = userdata;

    fprintf(stderr, "stream state: %s\n", pw_stream_state_as_string(state));

    switch (state) {
    case PW_STREAM_STATE_STREAMING:
        fprintf(stderr, "Streaming started!\n");
        break;
    case PW_STREAM_STATE_ERROR:
        fprintf(stderr, "Stream error: %s\n", error);
        pw_main_loop_quit(d->loop);
        break;
    default:
        break;
    }
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_stream_state_changed,
    .process = on_process,
};

static void signal_handler(int sig)
{
    pw_main_loop_quit(data.loop);
}

static bool setup_channel_map(struct data *d, const char *map_str)
{
    if (!map_str)
        return true;  // Use default mapping

    char *map_copy = strdup(map_str);
    char *token, *saveptr;
    int idx = 0;

    token = strtok_r(map_copy, ",", &saveptr);
    while (token && idx < CHANNELS) {
        int channel_pos = atoi(token);
        if (channel_pos >= 0 && channel_pos < CHANNELS) {
            // Map each logical channel to physical position
            d->info.position[idx] = channel_pos;
        } else {
            fprintf(stderr, "Invalid channel position %d (must be 0-%d)\n", 
                    channel_pos, CHANNELS-1);
            free(map_copy);
            return false;
        }
        token = strtok_r(NULL, ",", &saveptr);
        idx++;
    }

    free(map_copy);
    return true;
}

static void show_help(const char *name)
{
    fprintf(stderr, "Usage: %s [options] [file]\n", name);
    fprintf(stderr, "  -h            : show this help\n");
    fprintf(stderr, "  -r <rate>     : sample rate (default: %d)\n", RATE);
    fprintf(stderr, "  -c <channels> : number of channels (default: %d, max: %d)\n", CHANNELS, CHANNELS);
    fprintf(stderr, "  -m <map>      : channel map (comma-separated list of positions, e.g., '0,1,2,3,4,5,6,7,8,9,10,11')\n");
    fprintf(stderr, "  -v <volume>   : stream volume (default: 1.0)\n");
    fprintf(stderr, "  -n <name>     : stream name (default: 'multichannel-player')\n");
    fprintf(stderr, "  -t            : play test tone if no file is specified\n");
}

int main(int argc, char *argv[])
{
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    struct pw_properties *props;
    const char *channel_map = NULL;
    const char *stream_name = "multichannel-player";
    float volume = 1.0f;
    int c, n_channels = CHANNELS;
    int opt_test_tone = 0;
    int sample_rate = RATE;

    // Parse command-line options
    while ((c = getopt(argc, argv, "hc:m:v:n:tr:")) != -1) {
        switch (c) {
        case 'h':
            show_help(argv[0]);
            return 0;
        case 'c':
            n_channels = atoi(optarg);
            if (n_channels <= 0 || n_channels > CHANNELS) {
                fprintf(stderr, "Invalid channel count: %d (must be 1-%d)\n", n_channels, CHANNELS);
                return 1;
            }
            break;
        case 'm':
            channel_map = optarg;
            break;
        case 'v':
            volume = atof(optarg);
            break;
        case 'n':
            stream_name = optarg;
            break;
        case 't':
            opt_test_tone = 1;
            break;
        case 'r':
            sample_rate = atoi(optarg);
            if (sample_rate <= 0) {
                fprintf(stderr, "Invalid sample rate: %d\n", sample_rate);
                return 1;
            }
            break;
        default:
            fprintf(stderr, "Invalid option\n");
            show_help(argv[0]);
            return 1;
        }
    }

    // Initialize PipeWire
    pw_init(&argc, &argv);

    // Setup signal handling
    struct sigaction sa = { 0 };
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);

    // Initialize our data structure
    data.loop = pw_main_loop_new(NULL);
    if (data.loop == NULL) {
        fprintf(stderr, "Failed to create main loop\n");
        goto error;
    }

    // Setup properties
    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, stream_name,
        NULL);

    if (props == NULL) {
        fprintf(stderr, "Failed to create properties\n");
        goto error;
    }

    // Create a stream
    data.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        stream_name,
        props,
        &stream_events,
        &data);

    if (data.stream == NULL) {
        fprintf(stderr, "Failed to create stream\n");
        goto error;
    }

    // Setup audio format
    data.info = (struct spa_audio_info_raw) {
        .format = SPA_AUDIO_FORMAT_F32,
        .rate = sample_rate,
        .channels = n_channels,
    };

    // Default channel map: FL, FR, FC, LFE, RL, RR, SL, SR, TFL, TFR, TRL, TRR
    // (Front Left/Right, Front Center, LFE, Rear Left/Right, Side Left/Right, Top Front Left/Right, Top Rear Left/Right)
    data.info.position[0] = SPA_AUDIO_CHANNEL_AUX0;
    data.info.position[1] = SPA_AUDIO_CHANNEL_AUX1;
    data.info.position[2] = SPA_AUDIO_CHANNEL_AUX2;
    data.info.position[3] = SPA_AUDIO_CHANNEL_AUX3;
    data.info.position[4] = SPA_AUDIO_CHANNEL_AUX4;
    data.info.position[5] = SPA_AUDIO_CHANNEL_AUX5;
    data.info.position[6] = SPA_AUDIO_CHANNEL_AUX6;
    data.info.position[7] = SPA_AUDIO_CHANNEL_AUX7;
    data.info.position[8] = SPA_AUDIO_CHANNEL_AUX8;
    data.info.position[9] = SPA_AUDIO_CHANNEL_AUX9;
    data.info.position[10] = SPA_AUDIO_CHANNEL_AUX10;
    data.info.position[11] = SPA_AUDIO_CHANNEL_AUX11;

    // Apply custom channel mapping if provided
    if (!setup_channel_map(&data, channel_map)) {
        fprintf(stderr, "Failed to setup channel mapping\n");
        goto error;
    }

    // Load audio file if specified
    if (optind < argc) {
        data.filename = argv[optind];
        data.file = fopen(data.filename, "rb");
        if (data.file == NULL) {
            fprintf(stderr, "Failed to open file '%s': %s\n", data.filename, strerror(errno));
            goto error;
        }
        data.file_loaded = true;
        fprintf(stderr, "Playing file: %s\n", data.filename);
    } else if (opt_test_tone) {
        fprintf(stderr, "No file specified, playing test tone\n");
        data.file_loaded = false;
    } else {
        fprintf(stderr, "No input file specified and no test tone requested\n");
        show_help(argv[0]);
        goto error;
    }

    // Build the format params
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &data.info);
    if (params[0] == NULL) {
        fprintf(stderr, "Failed to build format parameters\n");
        goto error;
    }

    // Connect the stream
    if (pw_stream_connect(data.stream,
                         PW_DIRECTION_OUTPUT,
                         PW_ID_ANY,
                         PW_STREAM_FLAG_AUTOCONNECT |
                         PW_STREAM_FLAG_MAP_BUFFERS |
                         PW_STREAM_FLAG_RT_PROCESS,
                         params, 1) < 0) {
        fprintf(stderr, "Failed to connect stream\n");
        goto error;
    }

    // Set stream volume if needed
    if (volume != 1.0f) {
        struct spa_pod_builder b2 = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const struct spa_pod *params[1];
        params[0] = spa_pod_builder_add_object(&b2,
                SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
                SPA_PROP_volume, SPA_POD_Float(volume));
        pw_stream_update_params(data.stream, params, 1);
    }

    printf("Multichannel player started with %d channels\n", n_channels);
    printf("Press Ctrl+C to exit\n");

    // Run the main loop
    pw_main_loop_run(data.loop);

    // Cleanup
    if (data.file)
        fclose(data.file);
    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();

    return 0;

error:
    if (data.file)
        fclose(data.file);
    if (data.stream)
        pw_stream_destroy(data.stream);
    if (data.loop)
        pw_main_loop_destroy(data.loop);
    pw_deinit();

    return 1;
}
