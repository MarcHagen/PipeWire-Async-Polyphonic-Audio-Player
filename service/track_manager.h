#ifndef ASYNC_AUDIO_PLAYER_TRACK_MANAGER_H
#define ASYNC_AUDIO_PLAYER_TRACK_MANAGER_H

#include "types.h"
#include <spa/param/audio/raw.h>

// Track manager context
typedef struct track_manager_ctx track_manager_ctx_t;

// Helper function to convert port names to PipeWire channel positions
enum spa_audio_channel get_channel_position(const char *port_name);

// Initialize track manager
track_manager_ctx_t* track_manager_init(global_config_t *config);

// Cleanup track manager
void track_manager_cleanup(track_manager_ctx_t *ctx);

// Control functions
bool track_manager_play(track_manager_ctx_t *ctx, const char *track_id);
bool track_manager_stop(track_manager_ctx_t *ctx, const char *track_id);
bool track_manager_stop_all(track_manager_ctx_t *ctx);

// Status functions
bool track_manager_is_playing(track_manager_ctx_t *ctx, const char *track_id);
void track_manager_list_tracks(track_manager_ctx_t *ctx);
char* track_manager_print_status(track_manager_ctx_t *ctx);

// Test tone functionality
bool track_manager_play_test_tone(track_manager_ctx_t *ctx, const char *channel_mapping);

#endif // ASYNC_AUDIO_PLAYER_TRACK_MANAGER_H
