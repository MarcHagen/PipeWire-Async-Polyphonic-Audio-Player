#ifndef ASYNC_AUDIO_PLAYER_CONFIG_H
#define ASYNC_AUDIO_PLAYER_CONFIG_H

#include "types.h"

// Load configuration from YAML file
global_config_t* config_load(const char *filename);

// Free configuration structure
void config_free(global_config_t *config);

// Reload configuration
global_config_t* config_reload(const char *filename);

#endif // ASYNC_AUDIO_PLAYER_CONFIG_H
