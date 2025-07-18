#include <string.h>
#include <stdio.h>
#include "mqtt_client.h"
#include "log.h"

#define MQTT_KEEPALIVE 60
#define MQTT_QOS 2

struct mqtt_client_ctx {
    struct mosquitto *mosq;
    global_config_t *config;
    mqtt_command_callback_t command_cb;
    mqtt_connection_callback_t connection_cb;
    void *userdata;
    char *command_topic;
    char *status_topic;
    bool connected;
};

static void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    mqtt_client_ctx_t *ctx = (mqtt_client_ctx_t *) obj;

    if (rc == 0) {
        log_info("Connected to MQTT broker");
        ctx->connected = true;

        // Subscribe to command topic
        char topic[256];
        snprintf(topic, sizeof(topic), "%s/+/command", ctx->config->mqtt.topic_prefix);
        mosquitto_subscribe(mosq, NULL, topic, MQTT_QOS);

        // Publish online status
        mosquitto_publish(mosq, NULL, ctx->status_topic, 6, "online", MQTT_QOS, true);

        if (ctx->connection_cb) {
            ctx->connection_cb(true, ctx->userdata);
        }
    } else {
        log_error("Failed to connect to MQTT broker: %s", mosquitto_strerror(rc));
        ctx->connected = false;
        if (ctx->connection_cb) {
            ctx->connection_cb(false, ctx->userdata);
        }
    }
}

static void on_disconnect(
    struct mosquitto *mosq __attribute__((unused)),
    void *obj,
    int rc __attribute__((unused))
) {
    mqtt_client_ctx_t *ctx = (mqtt_client_ctx_t *) obj;
    log_warn("Disconnected from MQTT broker");
    ctx->connected = false;
    if (ctx->connection_cb) {
        ctx->connection_cb(false, ctx->userdata);
    }
}

static void on_message(struct mosquitto *mosq __attribute__((unused)), void *obj, const struct mosquitto_message *msg) {
    const mqtt_client_ctx_t *ctx = (mqtt_client_ctx_t *) obj;

    // Extract track_id from topic
    const char *prefix_end = strstr(msg->topic, ctx->config->mqtt.topic_prefix);
    if (!prefix_end) return;

    prefix_end += strlen(ctx->config->mqtt.topic_prefix) + 1; // Skip prefix and /
    const char *command_start = strstr(prefix_end, "/command");
    if (!command_start) return;

    char track_id[128] = {0};
    const size_t track_id_len = command_start - prefix_end;
    if (track_id_len >= sizeof(track_id)) return;
    strncpy(track_id, prefix_end, track_id_len);

    // Handle command
    if (ctx->command_cb) {
        ctx->command_cb(track_id, (const char *) msg->payload, ctx->userdata);
    }
}

mqtt_client_ctx_t *mqtt_client_init(
    global_config_t *config,
    const mqtt_command_callback_t command_cb,
    const mqtt_connection_callback_t connection_cb,
    void *userdata
) {
    mqtt_client_ctx_t *ctx = calloc(1, sizeof(mqtt_client_ctx_t));
    if (!ctx) {
        log_error("Failed to allocate MQTT context");
        return NULL;
    }

    ctx->config = config;
    ctx->command_cb = command_cb;
    ctx->connection_cb = connection_cb;
    ctx->userdata = userdata;

    // Initialize status topic
    size_t topic_len = strlen(config->mqtt.topic_prefix) + 7; // /status + null
    ctx->status_topic = malloc(topic_len);
    if (!ctx->status_topic) {
        free(ctx);
        return NULL;
    }
    snprintf(ctx->status_topic, topic_len, "%s/status", config->mqtt.topic_prefix);

    // Initialize mosquitto library
    mosquitto_lib_init();

    // Create new mosquitto instance
    ctx->mosq = mosquitto_new(config->mqtt.client_id, true, ctx);
    if (!ctx->mosq) {
        log_error("Failed to create mosquitto instance");
        free(ctx->status_topic);
        free(ctx);
        return NULL;
    }

    // Set up will message (offline status)
    mosquitto_will_set(ctx->mosq, ctx->status_topic, 7, "offline", MQTT_QOS, true);

    // Set up callbacks
    mosquitto_connect_callback_set(ctx->mosq, on_connect);
    mosquitto_disconnect_callback_set(ctx->mosq, on_disconnect);
    mosquitto_message_callback_set(ctx->mosq, on_message);

    // Set up credentials if provided
    if (config->mqtt.username && config->mqtt.password) {
        mosquitto_username_pw_set(
            ctx->mosq,
            config->mqtt.username,
            config->mqtt.password
        );
    }

    return ctx;
}

bool mqtt_client_start(mqtt_client_ctx_t *ctx) {
    if (!ctx) return false;

    // Connect to broker
    int rc = mosquitto_connect(
        ctx->mosq,
        ctx->config->mqtt.host,
        ctx->config->mqtt.port,
        MQTT_KEEPALIVE
    );

    if (rc != MOSQ_ERR_SUCCESS) {
        log_error("Failed to connect to MQTT broker: %s", mosquitto_strerror(rc));
        return false;
    }

    // Start network loop
    rc = mosquitto_loop_start(ctx->mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        log_error("Failed to start MQTT network loop: %s", mosquitto_strerror(rc));
        return false;
    }

    return true;
}

void mqtt_client_stop(mqtt_client_ctx_t *ctx) {
    if (!ctx) return;

    if (ctx->mosq) {
        mosquitto_publish(ctx->mosq, NULL, ctx->status_topic, 7, "offline", MQTT_QOS, true);
        mosquitto_disconnect(ctx->mosq);
        mosquitto_loop_stop(ctx->mosq, true);
    }
}

void mqtt_client_publish_status(mqtt_client_ctx_t *ctx, const char *track_id, bool playing) {
    if (!ctx || !ctx->connected || !track_id) return;

    char topic[256];
    snprintf(
        topic,
        sizeof(topic),
        "%s/%s/status",
        ctx->config->mqtt.topic_prefix,
        track_id
    );

    const char *status = playing ? "playing" : "stopped";
    mosquitto_publish(ctx->mosq, NULL, topic, strlen(status), status, MQTT_QOS, false);
}

void mqtt_client_cleanup(mqtt_client_ctx_t *ctx) {
    if (!ctx) return;

    if (ctx->mosq) {
        mqtt_client_stop(ctx);
        mosquitto_destroy(ctx->mosq);
    }

    free(ctx->status_topic);
    free(ctx);
    mosquitto_lib_cleanup();
}
