#ifndef ASYNC_AUDIO_PLAYER_DAEMON_H
#define ASYNC_AUDIO_PLAYER_DAEMON_H

#include <stdbool.h>

// Daemonize the process
bool daemonize(const char *working_dir);

#endif // ASYNC_AUDIO_PLAYER_DAEMON_H
