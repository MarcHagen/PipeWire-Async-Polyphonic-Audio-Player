#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "cli.h"
#include "log.h"

static struct option long_options[] = {
    {"list",    no_argument,       0, 'l'},
    {"play",    required_argument, 0, 'p'},
    {"stop",    required_argument, 0, 's'},
    {"stopall", no_argument,       0, 'a'},
    {"reload",  no_argument,       0, 'r'},
    {"status",  no_argument,       0, 't'},
    {"test",    no_argument,       0, 'e'},
    {"help",    no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

void cli_print_help(const char *program_name) {
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  --list                List all configured tracks\n");
    printf("  --play <track_id>     Play a track\n");
    printf("  --stop <track_id>     Stop a track\n");
    printf("  --stopall             Stop all tracks\n");
    printf("  --reload              Reload configuration\n");
    printf("  --status              Show current status\n");
    printf("  --test                Run test tone\n");
    printf("  --help                Show this help message\n");
}

cli_args_t cli_parse_args(int argc, char *argv[]) {
    cli_args_t args = {
        .command = CMD_NONE,
        .track_id = NULL
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "lp:s:arth", 
                           long_options, &option_index)) != -1) {
        switch (c) {
            case 'l':
                args.command = CMD_LIST;
                break;
            case 'p':
                args.command = CMD_PLAY;
                args.track_id = strdup(optarg);
                break;
            case 's':
                args.command = CMD_STOP;
                args.track_id = strdup(optarg);
                break;
            case 'a':
                args.command = CMD_STOPALL;
                break;
            case 'r':
                args.command = CMD_RELOAD;
                break;
            case 't':
                args.command = CMD_STATUS;
                break;
            case 'e':
                args.command = CMD_TEST;
                break;
            case 'h':
                args.command = CMD_HELP;
                break;
            case '?':
                args.command = CMD_HELP;
                break;
        }
    }

    return args;
}
