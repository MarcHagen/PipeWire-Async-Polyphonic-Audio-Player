#include <stdlib.h>
#include <string.h>
#include "audio_file.h"
#include "log.h"

#define BUFFER_FRAMES 4096

audio_file_t *audio_file_open(const char *path, const bool loop, const float volume) {
    audio_file_t *af = calloc(1, sizeof(audio_file_t));
    if (!af) {
        log_error("Failed to allocate audio file structure");
        return NULL;
    }

    // Initialize SF_INFO structure
    memset(&af->info, 0, sizeof(SF_INFO));

    // Open the sound file
    af->file = sf_open(path, SFM_READ, &af->info);
    if (!af->file) {
        log_error("Failed to open audio file: %s (%s)", path, sf_strerror(NULL));
        free(af);
        return NULL;
    }

    // Allocate buffer
    af->buffer_size = BUFFER_FRAMES * af->info.channels;
    af->buffer = malloc(af->buffer_size * sizeof(float));
    if (!af->buffer) {
        log_error("Failed to allocate audio buffer");
        sf_close(af->file);
        free(af);
        return NULL;
    }

    af->loop = loop;
    af->volume = volume;
    af->position = 0;

    log_info("Opened audio file: %s (channels: %d, rate: %d)",
             path, af->info.channels, af->info.samplerate);

    return af;
}

size_t audio_file_read(audio_file_t *af, float *output, const size_t frames) {
    if (!af || !output) return 0;

    size_t frames_read = sf_readf_float(af->file, output, frames);

    // Apply volume
    if (af->volume != 1.0f) {
        for (size_t i = 0; i < frames_read * af->info.channels; i++) {
            output[i] *= af->volume;
        }
    }

    // Handle looping
    if (frames_read < frames && af->loop) {
        sf_seek(af->file, 0, SEEK_SET);
        const size_t remaining = frames - frames_read;
        const size_t additional = sf_readf_float(af->file, output + (frames_read * af->info.channels), remaining);
        frames_read += additional;
    }

    af->position += frames_read;
    return frames_read;
}

bool audio_file_seek(audio_file_t *af, const sf_count_t position) {
    if (!af) return false;

    const sf_count_t result = sf_seek(af->file, position, SEEK_SET);
    if (result < 0) {
        log_error("Failed to seek in audio file: %s", sf_strerror(af->file));
        return false;
    }

    af->position = position;
    return true;
}

void audio_file_close(audio_file_t *af) {
    if (!af) return;

    if (af->file) {
        sf_close(af->file);
    }
    free(af->buffer);
    free(af);
}
