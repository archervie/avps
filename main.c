#include "connect_scan.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int opt;
    int option_index = 0;
    int start_port = 1;
    int end_port = 65535;
    float timeout = 1.0;
    int success_only = 0;
    const char *host = NULL;

    static struct option long_options[] = {
        {"success-only", no_argument, 0, 's'},
        {"range", required_argument, 0, 'r'},
        {"timeout", required_argument, 0, 't'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "r:t:s", long_options,
                              &option_index)) != -1) {

        switch (opt) {
        case 'r':
            if (sscanf(optarg, "%d-%d", &start_port, &end_port) != 2 ||
                end_port < start_port) {
                fprintf(stderr, "Invalid range, use start-end (e.g. 1-1000) "
                                "with end > start.\n");
                exit(EXIT_FAILURE);
            }
            break;

        case 't':
            timeout = atof(optarg);
            break;

        case 's':
            success_only = 1;
            break;

        default:
            fprintf(stderr,
                    "Usage: %s [-r --range start-end] [-t --timeout timeout] "
                    "host\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Host argument is required.\n");
        fprintf(
            stderr,
            "Usage: %s [-r --range start-end] [-t --timeout timeout] host\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }
    host = argv[optind];

    printf("Scanning %s from port %d to %d with a %.2fs timeout...\n", host,
           start_port, end_port, timeout);
    connect_scanner(argv[1], host, start_port, end_port, timeout, success_only);
    exit(EXIT_SUCCESS);
}
