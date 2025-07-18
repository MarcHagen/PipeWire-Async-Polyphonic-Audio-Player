#ifndef ASYNC_AUDIO_PLAYER_SOCKET_SERVER_H
#define ASYNC_AUDIO_PLAYER_SOCKET_SERVER_H

#include <stdbool.h>
#include <pthread.h>
#include "track_manager.h"

// Socket server context
typedef struct {
    track_manager_ctx_t *track_manager;
    pthread_t thread;
    int server_fd;
    bool running;
    char socket_path[256];  // Added to store the actual path
} socket_server_ctx_t;

// Get the socket path for the current user
char* get_socket_path(char* buffer, size_t size);

// Initialize socket server
socket_server_ctx_t *socket_server_init(track_manager_ctx_t *track_manager);

// Start socket server thread
bool socket_server_start(socket_server_ctx_t *ctx);

// Stop and cleanup socket server
void socket_server_cleanup(socket_server_ctx_t *ctx);

#endif // ASYNC_AUDIO_PLAYER_SOCKET_SERVER_H
