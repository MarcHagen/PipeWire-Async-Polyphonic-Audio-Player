#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "types.h"
#include "log.h"
#include "config.h"
#include "cli.h"
#include "track_manager.h"
#include "mqtt_client.h"
#include "mqtt_client.h"

// Forward declarations for MQTT callbacks
static void handle_mqtt_command(const char *track_id, const char *command, void *userdata);
static void handle_mqtt_connection(bool connected, void *userdata);

// Configuration file search paths
static const char *CONFIG_PATHS[] = {
    "./config/default.yml",
    "./default.yml",
    "/etc/async-audio-player/config.yml",
    NULL
};

// Find existing configuration file
static const char* find_config_file(void) {
    for (const char **path = CONFIG_PATHS; *path != NULL; path++) {
        if (access(*path, R_OK) == 0) {
            return *path;
        }
    }
    return NULL;
}

// Global state
static bool g_running = true;
static global_config_t *g_config = NULL;
static track_manager_ctx_t *g_track_manager = NULL;

// Signal handler
static void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        log_info("Received shutdown signal");
        g_running = false;
    }
}

// Initialize signal handlers
// MQTT command handler
static void handle_mqtt_command(const char *track_id, const char *command, void *userdata) {
    track_manager_ctx_t *track_manager = (track_manager_ctx_t *)userdata;

    if (strcmp(command, "play") == 0) {
        track_manager_play(track_manager, track_id);
    } else if (strcmp(command, "stop") == 0) {
        track_manager_stop(track_manager, track_id);
    } else if (strcmp(command, "stopall") == 0) {
        track_manager_stop_all(track_manager);
    }
}

// MQTT connection handler
static void handle_mqtt_connection(bool connected, void *userdata __attribute__((unused))) {
    if (connected) {
        log_info("MQTT connection established");
    } else {
        log_warn("MQTT connection lost");
    }
}

static bool init_signals(void) {
    struct sigaction sa = { 0 };
    sa.sa_handler = signal_handler;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        log_error("Failed to set up SIGINT handler");
        return false;
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        log_error("Failed to set up SIGTERM handler");
        return false;
    }

    return true;
}

// Program entry point
int main(int argc, char *argv[]) {
    // Parse command line arguments
    cli_args_t args = cli_parse_args(argc, argv);

    // Handle help command early
    if (args.command == CMD_HELP) {
        cli_print_help(argv[0]);
        return EXIT_SUCCESS;
    }

    // Initialize logging
    log_set_level("INFO");
    log_info("Async Audio Player starting...");

    // Initialize signal handlers
    if (!init_signals()) {
        return EXIT_FAILURE;
    }

    // Find and load configuration
    const char *config_path = find_config_file();
    if (!config_path) {
        log_error("No configuration file found. Searched in:");
        for (const char **path = CONFIG_PATHS; *path != NULL; path++) {
            log_error("  %s", *path);
        }
        return EXIT_FAILURE;
    }

    log_info("Using configuration file: %s", config_path);
    g_config = config_load(config_path);
    if (!g_config) {
        log_error("Failed to load configuration");
        return EXIT_FAILURE;
    }

    // Apply logging level from config
    if (g_config->logging.level) {
        log_set_level(g_config->logging.level);
    }

    // Initialize track manager
    g_track_manager = track_manager_init(g_config);
    if (!g_track_manager) {
        log_error("Failed to initialize track manager");
        config_free(g_config);
        return EXIT_FAILURE;
    }

    // Initialize MQTT if enabled
    mqtt_client_ctx_t *mqtt_ctx = NULL;
    if (g_config->mqtt.enabled) {
        mqtt_ctx = mqtt_client_init(g_config,
            // Command callback
            (mqtt_command_callback_t)handle_mqtt_command,
            // Connection callback
            (mqtt_connection_callback_t)handle_mqtt_connection,
            g_track_manager);

        if (!mqtt_ctx) {
            log_error("Failed to initialize MQTT client");
            track_manager_cleanup(g_track_manager);
            config_free(g_config);
            return EXIT_FAILURE;
        }

        if (!mqtt_client_start(mqtt_ctx)) {
            log_error("Failed to start MQTT client");
            mqtt_client_cleanup(mqtt_ctx);
            track_manager_cleanup(g_track_manager);
            config_free(g_config);
            return EXIT_FAILURE;
        }
    }

    log_info("Initialization complete");

    // Handle CLI commands
    switch (args.command) {
        case CMD_NONE:
            // No command specified, just run in daemon mode
            break;
        case CMD_HELP:
            cli_print_help(argv[0]);
            return EXIT_SUCCESS;
        case CMD_LIST:
            track_manager_list_tracks(g_track_manager);
            break;
        case CMD_PLAY:
            if (args.track_id) {
                if (!track_manager_play(g_track_manager, args.track_id)) {
                    log_error("Failed to play track: %s", args.track_id);
                }
            }
            break;
        case CMD_STOP:
            if (args.track_id) {
                if (!track_manager_stop(g_track_manager, args.track_id)) {
                    log_error("Failed to stop track: %s", args.track_id);
                }
            }
            break;
        case CMD_STOPALL:
            if (!track_manager_stop_all(g_track_manager)) {
                log_error("Failed to stop all tracks");
            }
            break;
        case CMD_RELOAD:
            {
                const char *reload_path = find_config_file();
                if (!reload_path) {
                    log_error("Configuration file not found for reload");
                    break;
                }

                global_config_t *new_config = config_reload(reload_path);
                if (new_config) {
                    track_manager_stop_all(g_track_manager);
                    track_manager_cleanup(g_track_manager);
                    config_free(g_config);
                    g_config = new_config;
                    g_track_manager = track_manager_init(g_config);
                    if (!g_track_manager) {
                        log_error("Failed to reinitialize track manager");
                        config_free(g_config);
                        return EXIT_FAILURE;
                    }
                    log_info("Configuration reloaded successfully");
                } else {
                    log_error("Failed to reload configuration");
                }
            }
            break;
        case CMD_STATUS:
            track_manager_print_status(g_track_manager);
            break;
        case CMD_TEST:
            if (!track_manager_play_test_tone(g_track_manager)) {
                log_error("Failed to play test tone");
            }
            break;
    }

    // Main loop
    while (g_running) {
        // TODO: Process MQTT messages
        usleep(100000); // 100ms sleep to prevent busy loop
    }

    // Cleanup
    log_info("Shutting down...");
    if (args.track_id) {
        free(args.track_id);
    }
    if (mqtt_ctx) {
        mqtt_client_cleanup(mqtt_ctx);
    }
    if (g_track_manager) {
        track_manager_cleanup(g_track_manager);
    }
    if (g_config) {
        config_free(g_config);
    }

    return EXIT_SUCCESS;
}
