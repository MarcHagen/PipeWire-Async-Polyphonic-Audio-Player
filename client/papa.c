#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>
#include "pw_monitor.h"

// Socket path definition
#define BUFFER_SIZE 1024

// Command line options
static struct option long_options[] = {
    {"list", no_argument, 0, 'l'},
    {"play", required_argument, 0, 'p'},
    {"stop", required_argument, 0, 's'},
    {"stop-all", no_argument, 0, 'a'},
    {"reload", no_argument, 0, 'r'},
    {"status", no_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},
    {"list-devices", no_argument, 0, 'd'},
    {0, 0, 0, 0}
};

// Print help message
static void print_help(const char *program_name) {
    printf("PAPA - PipeWire Async Polyphonic Audio Player Client\n\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  --list                List all configured tracks\n");
    printf("  --play <track_id>     Play a track\n");
    printf("  --stop <track_id>     Stop a track\n");
    printf("  --stop-all            Stop all tracks\n");
    printf("  --reload              Reload configuration\n");
    printf("  --status              Show current status\n");
    printf("  --list-devices        List available PipeWire audio devices\n");
    printf("  --help                Show this help message\n");
}

// Get the socket path for the current user
static char* get_socket_path(char* buffer, size_t size)
{
    if (!buffer || size == 0)
    {
        return NULL;
    }

    // Ensure the runtime directory exists
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "/var/run/user/%d/papa", (int)getuid());

    // Construct the socket path
    snprintf(buffer, size, "%s/papad.sock", dir_path);
    return buffer;
}


// Send command to socket server
static int send_command(const char *command) {
    int sock;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];

    char socket_path[256];
    get_socket_path(socket_path, sizeof(socket_path));

    // Create socket
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Setup address structure
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    // Connect to server
    if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
        perror("connect");
        fprintf(stderr, "Error: Could not connect to audio player. Is it running?\n");
        close(sock);
        return EXIT_FAILURE;
    }

    // Send command
    if (write(sock, command, strlen(command)) < 0) {
        perror("write");
        close(sock);
        return EXIT_FAILURE;
    }

    // Receive response
    ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s\n", buffer);
    }

    close(sock);
    return EXIT_SUCCESS;
}

// Main function for client mode
int main(int argc, char *argv[]) {
    int option_index = 0;
    int c;

    // Handle help command early
    if (argc <= 1) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }

    // Parse command line arguments
    while ((c = getopt_long(argc, argv, "lp:s:arth", long_options, &option_index)) != -1) {
        switch (c) {
            case 'l':
                return send_command("list");
            case 'p':
                if (optarg) {
                    char command[BUFFER_SIZE];
                    snprintf(command, sizeof(command), "play %s", optarg);
                    return send_command(command);
                }
                fprintf(stderr, "Error: --play requires a track ID\n");
                return EXIT_FAILURE;
            case 's':
                if (optarg) {
                    char command[BUFFER_SIZE];
                    snprintf(command, sizeof(command), "stop %s", optarg);
                    return send_command(command);
                }
                fprintf(stderr, "Error: --stop requires a track ID\n");
                return EXIT_FAILURE;
            case 'a':
                return send_command("stop-all");
            case 'r':
                return send_command("reload");
            case 't':
                return send_command("status");
            case 'h':
                print_help(argv[0]);
                return EXIT_SUCCESS;
            case '?':
                print_help(argv[0]);
                return EXIT_FAILURE;
            case 'd':
                list_audio_devices();
                return EXIT_SUCCESS;
        }
    }

    print_help(argv[0]);
    return EXIT_SUCCESS;
}
