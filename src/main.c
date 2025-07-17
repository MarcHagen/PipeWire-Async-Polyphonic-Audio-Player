#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "types.h"
#include "log.h"
#include "config.h"
#include "cli.h"

#define DEFAULT_CONFIG_PATH "/etc/async-audio-player/config.yml"

// Global state
static bool g_running = true;
static global_config_t *g_config = NULL;

// Signal handler
static void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        log_info("Received shutdown signal");
        g_running = false;
    }
}

// Initialize signal handlers
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

    // Load configuration
    g_config = config_load(DEFAULT_CONFIG_PATH);
    if (!g_config) {
        log_error("Failed to load configuration");
        return EXIT_FAILURE;
    }

    // Apply logging level from config
    if (g_config->logging.level) {
        log_set_level(g_config->logging.level);
    }

    // TODO: Initialize MQTT
    // TODO: Initialize audio system

    log_info("Initialization complete");

    // Handle CLI commands
    switch (args.command) {
        case CMD_LIST:
            // TODO: Implement list tracks
            break;
        case CMD_PLAY:
            if (args.track_id) {
                // TODO: Implement play track
                log_info("Playing track: %s", args.track_id);
            }
            break;
        case CMD_STOP:
            if (args.track_id) {
                // TODO: Implement stop track
                log_info("Stopping track: %s", args.track_id);
            }
            break;
        case CMD_STOPALL:
            // TODO: Implement stop all tracks
            log_info("Stopping all tracks");
            break;
        case CMD_RELOAD:
            // TODO: Implement configuration reload
            log_info("Reloading configuration");
            break;
        case CMD_STATUS:
            // TODO: Implement status display
            log_info("Displaying status");
            break;
        case CMD_TEST:
            // TODO: Implement test tone
            log_info("Playing test tone");
            break;
    }

    // Main loop
    while (g_running) {
        // TODO: Process messages and maintain state
        usleep(100000); // 100ms sleep to prevent busy loop
    }

    // Cleanup
    log_info("Shutting down...");
    if (args.track_id) {
        free(args.track_id);
    }
    if (g_config) {
        config_free(g_config);
    }

    return EXIT_SUCCESS;
}
