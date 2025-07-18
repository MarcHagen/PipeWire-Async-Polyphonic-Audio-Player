#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "types.h"
#include "log.h"
#include "config.h"
#include "track_manager.h"
#include "signal_handler.h"
#include "socket_server.h"

#define PID_FILE "/var/run/papad.pid"

// Create PID file
static bool create_pid_file(void) {
    FILE *file = fopen(PID_FILE, "w");
    if (!file) {
        log_error("Failed to create PID file: %s", PID_FILE);
        return false;
    }

    fprintf(file, "%d\n", getpid());
    fclose(file);

    log_info("Created PID file: %s", PID_FILE);
    return true;
}

// Remove PID file
static void remove_pid_file(void) {
    unlink(PID_FILE);
    log_info("Removed PID file: %s", PID_FILE);
}

// Configuration file search paths
static const char *CONFIG_PATHS[] = {
    "./config/default.yml",
    "./default.yml",
    "/etc/papa/config.yml",
    NULL
};

// Find existing configuration file
static const char *find_config_file(void) {
    for (const char **path = CONFIG_PATHS; *path != NULL; path++) {
        if (access(*path, R_OK) == 0) {
            return *path;
        }
    }
    return NULL;
}

// Global state
static global_config_t *g_config = NULL;
static track_manager_ctx_t *g_track_manager = NULL;
static socket_server_ctx_t *g_socket_server = NULL;


// Program entry point
int main(const int argc, char *argv[]) {
    int returnInt = EXIT_SUCCESS;

    // Initialize logging
    log_set_level("INFO");
    log_info("PAPA - PipeWire Async Polyphonic Audio Player starting...");

    // Print help if requested
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("PAPA - PipeWire Async Polyphonic Audio Player\n");
            printf("Usage: %s [--help]\n\n", argv[0]);
            printf("This program runs as a server. To control it, use the 'papa' client utility.\n");
            printf("The server listens for commands on the Unix socket at %s\n", SOCKET_PATH);
            return EXIT_SUCCESS;
        }
    }

    // Initialize signal handlers
    if (!signal_handler_init()) {
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
        returnInt = EXIT_FAILURE;
        goto cleanup;
    }

    log_info("Initialization complete");

    // Create PID file
    if (!create_pid_file()) {
        log_warn("Failed to create PID file - continuing anyway");
    }

    // Initialize socket server
    g_socket_server = socket_server_init(g_track_manager);
    if (!g_socket_server) {
        log_error("Failed to initialize socket server");
        returnInt = EXIT_FAILURE;
        goto cleanup;
    }

    // Start socket server
    if (!socket_server_start(g_socket_server)) {
        log_error("Failed to start socket server");
        returnInt = EXIT_FAILURE;
        goto cleanup;
    }

    // Main loop
    bool running = true;
    while (running) {
        const signal_state_t sig_state = signal_handler_get_state();

        switch (sig_state) {
            case SIGNAL_SHUTDOWN:
                log_info("Received shutdown signal");
                running = false;
                break;

            case SIGNAL_RELOAD:
                log_info("Reloading configuration");
                {
                    const char *reload_path = find_config_file();
                    if (!reload_path) {
                        log_error("Configuration file not found for reload");
                    } else {
                        global_config_t *new_config = config_reload(reload_path);
                        if (new_config) {
                            track_manager_stop_all(g_track_manager);
                            track_manager_cleanup(g_track_manager);
                            config_free(g_config);
                            g_config = new_config;

                            g_track_manager = track_manager_init(g_config);
                            if (!g_track_manager) {
                                log_error("Failed to reinitialize track manager");
                                running = false;
                                break;
                            }

                            log_info("Configuration reloaded successfully");
                        } else {
                            log_error("Failed to reload configuration");
                        }
                    }
                }
                signal_handler_reset();
                break;

            case SIGNAL_NONE:
            default:
                usleep(100000); // 100 ms sleep to prevent heavy loop
                break;
        }
    }

    returnInt = EXIT_SUCCESS;
    log_info("Shutting down...");

cleanup:
    if (g_socket_server) {
        socket_server_cleanup(g_socket_server);
    }
    if (g_track_manager) {
        track_manager_cleanup(g_track_manager);
    }
    if (g_config) {
        config_free(g_config);
    }

    remove_pid_file();
    signal_handler_cleanup();
    return returnInt;
}
