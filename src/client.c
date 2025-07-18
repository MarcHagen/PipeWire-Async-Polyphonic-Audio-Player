#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>

// Use the same socket path definition as the server
#define SOCKET_PATH "/var/run/async-audio-player.sock"
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
    {0, 0, 0, 0}
};

// Print help message
static void print_help(const char *program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  --list                List all configured tracks\n");
    printf("  --play <track_id>     Play a track\n");
    printf("  --stop <track_id>     Stop a track\n");
    printf("  --stop-all            Stop all tracks\n");
    printf("  --reload              Reload configuration\n");
    printf("  --status              Show current status\n");
    printf("  --help                Show this help message\n");
}

// Send command to socket server
static int send_command(const char *command) {
    int sock;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Setup address structure
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

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
        }
    }

    print_help(argv[0]);
    return EXIT_SUCCESS;
}
