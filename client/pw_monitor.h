#ifndef ASYNC_AUDIO_PLAYER_PW_MONITOR_H
#define ASYNC_AUDIO_PLAYER_PW_MONITOR_H

#include <pipewire/pipewire.h>

// Structure to store node information
typedef struct {
    uint32_t id;
    const char *name;
    const char *description;
    const char *class;
} pw_node_info_t;

// Function to list all audio devices
void list_audio_devices(void);

#endif // ASYNC_AUDIO_PLAYER_PW_MONITOR_H
