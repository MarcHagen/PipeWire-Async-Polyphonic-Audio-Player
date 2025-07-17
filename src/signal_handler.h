#ifndef ASYNC_AUDIO_PLAYER_SIGNAL_HANDLER_H
#define ASYNC_AUDIO_PLAYER_SIGNAL_HANDLER_H

#include "types.h"

// Initialize signal handling
bool signal_handler_init(void);

// Get current signal state
signal_state_t signal_handler_get_state(void);

// Reset signal state
void signal_handler_reset(void);

// Clean up signal handling
void signal_handler_cleanup(void);

#endif // ASYNC_AUDIO_PLAYER_SIGNAL_HANDLER_H
