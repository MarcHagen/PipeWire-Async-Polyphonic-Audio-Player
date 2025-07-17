#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sndfile.h>

#define SAMPLE_RATE 48000
#define DURATION 5  // seconds
#define NUM_CHANNELS 2

int main(void) {
    SF_INFO sf_info = {
        .samplerate = SAMPLE_RATE,
        .channels = NUM_CHANNELS,
        .format = SF_FORMAT_WAV | SF_FORMAT_FLOAT
    };

    SNDFILE *file = sf_open("/tmp/test.wav", SFM_WRITE, &sf_info);
    if (!file) {
        fprintf(stderr, "Error: Could not open file for writing\n");
        return 1;
    }

    // Generate a test tone (440Hz sine wave)
    float *buffer = malloc(SAMPLE_RATE * NUM_CHANNELS * sizeof(float));
    if (!buffer) {
        fprintf(stderr, "Error: Could not allocate buffer\n");
        sf_close(file);
        return 1;
    }

    for (int t = 0; t < DURATION * SAMPLE_RATE; t++) {
        float sample = sin(2.0 * M_PI * 440.0 * t / SAMPLE_RATE) * 0.5;
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            buffer[t * NUM_CHANNELS + ch] = sample;
        }
    }

    sf_writef_float(file, buffer, DURATION * SAMPLE_RATE);

    free(buffer);
    sf_close(file);

    printf("Generated test WAV file: /tmp/test.wav\n");
    printf("Duration: %d seconds\n", DURATION);
    printf("Channels: %d\n", NUM_CHANNELS);
    printf("Sample rate: %d Hz\n", SAMPLE_RATE);

    return 0;
}
