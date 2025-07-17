#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "types.h"
#include "log.h"

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
    // Initialize logging
    log_set_level("INFO");
    log_info("Async Audio Player starting...");

    // Initialize signal handlers
    if (!init_signals()) {
        return EXIT_FAILURE;
    }

    // TODO: Parse command line arguments
    // TODO: Load configuration
    // TODO: Initialize MQTT
    // TODO: Initialize audio system

    log_info("Initialization complete");

    // Main loop
    while (g_running) {
        // TODO: Process messages and maintain state
        usleep(100000); // 100ms sleep to prevent busy loop
    }

    // Cleanup
    log_info("Shutting down...");
    // TODO: Cleanup resources

    return EXIT_SUCCESS;
}
