#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "daemon.h"
#include "log.h"

bool daemonize(const char *working_dir) {
    pid_t pid;

    // First fork (detaches from parent)
    pid = fork();
    if (pid < 0) {
        log_error("Failed to fork daemon process");
        return false;
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); // Parent exits
    }

    // Create new session
    if (setsid() < 0) {
        log_error("Failed to create new session");
        return false;
    }

    // Second fork (relinquishes session leadership)
    pid = fork();
    if (pid < 0) {
        log_error("Failed to fork daemon process");
        return false;
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Set new file permissions
    umask(0);

    // Change to working directory
    if (working_dir && chdir(working_dir) < 0) {
        log_error("Failed to change working directory to: %s", working_dir);
        return false;
    }

    // Close all open file descriptors
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }

    // Redirect standard file descriptors to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        log_error("Failed to open /dev/null");
        return false;
    }

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    if (fd > 2) {
        close(fd);
    }

    return true;
}
