#ifndef ASYNC_AUDIO_PLAYER_MQTT_CLIENT_H
#define ASYNC_AUDIO_PLAYER_MQTT_CLIENT_H

#include <stdbool.h>
#include <mosquitto.h>
#include "types.h"

typedef struct mqtt_client_ctx mqtt_client_ctx_t;

// MQTT client callbacks
typedef void (*mqtt_command_callback_t)(const char *track_id, const char *command, void *userdata);
typedef void (*mqtt_connection_callback_t)(bool connected, void *userdata);

// Initialize MQTT client
mqtt_client_ctx_t* mqtt_client_init(global_config_t *config,
                                  mqtt_command_callback_t command_cb,
                                  mqtt_connection_callback_t connection_cb,
                                  void *userdata);

// Start MQTT client
bool mqtt_client_start(mqtt_client_ctx_t *ctx);

// Stop MQTT client
void mqtt_client_stop(mqtt_client_ctx_t *ctx);

// Publish track status
void mqtt_client_publish_status(mqtt_client_ctx_t *ctx, const char *track_id, bool playing);

// Clean up MQTT client
void mqtt_client_cleanup(mqtt_client_ctx_t *ctx);

#endif // ASYNC_AUDIO_PLAYER_MQTT_CLIENT_H
