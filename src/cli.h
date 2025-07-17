#ifndef ASYNC_AUDIO_PLAYER_CLI_H
#define ASYNC_AUDIO_PLAYER_CLI_H

#include <stdbool.h>

// CLI command types
typedef enum {
    CMD_NONE,
    CMD_LIST,
    CMD_PLAY,
    CMD_STOP,
    CMD_STOPALL,
    CMD_RELOAD,
    CMD_STATUS,
    CMD_TEST,
    CMD_HELP
} cli_command_t;

// CLI command arguments
typedef struct {
    cli_command_t command;
    char *track_id;
} cli_args_t;

// Parse command line arguments
cli_args_t cli_parse_args(int argc, char *argv[]);

// Print help message
void cli_print_help(const char *program_name);

#endif // ASYNC_AUDIO_PLAYER_CLI_H
