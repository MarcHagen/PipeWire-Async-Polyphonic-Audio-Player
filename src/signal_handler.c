#include <stdio.h>
#include <signal.h>
#include "types.h"
#include "signal_handler.h"
#include "log.h"

static volatile sig_atomic_t signal_received = 0;
static volatile signal_state_t current_state = SIGNAL_NONE;

static void handle_signal(int signo) {
    signal_received = signo;

    switch (signo) {
        case SIGINT:
        case SIGTERM:
            current_state = SIGNAL_SHUTDOWN;
            break;
        case SIGUSR1:
            current_state = SIGNAL_RELOAD;
            break;
    }
}

bool signal_handler_init(void) {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Add signals to mask while handler is running
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGUSR1);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        log_error("Failed to set up SIGINT handler");
        return false;
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        log_error("Failed to set up SIGTERM handler");
        return false;
    }

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        log_error("Failed to set up SIGUSR1 handler");
        return false;
    }

    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    return true;
}

signal_state_t signal_handler_get_state(void) {
    return current_state;
}

void signal_handler_reset(void) {
    signal_received = 0;
    current_state = SIGNAL_NONE;
}

void signal_handler_cleanup(void) {
    // Restore default signal handlers
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}
