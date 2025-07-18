#ifndef ASYNC_AUDIO_PLAYER_AUDIO_FILE_H
#define ASYNC_AUDIO_PLAYER_AUDIO_FILE_H

#include <sndfile.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    SNDFILE *file;
    SF_INFO info;
    float *buffer;
    size_t buffer_size;
    bool loop;
    float volume;
    sf_count_t position;
} audio_file_t;

// Open audio file and prepare for reading
audio_file_t* audio_file_open(const char *path, bool loop, float volume);

// Read next chunk of audio data
size_t audio_file_read(audio_file_t *af, float *output, size_t frames);

// Seek to position in file
bool audio_file_seek(audio_file_t *af, sf_count_t position);

// Close audio file and free resources
void audio_file_close(audio_file_t *af);

#endif // ASYNC_AUDIO_PLAYER_AUDIO_FILE_H
