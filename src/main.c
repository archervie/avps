#include "connect_scan.h"
#include "stealth_scan.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int opt;
    int option_index = 0;
    int mode = -1;
    int start_port = 1;
    int end_port = 65535;
    float timeout = 1.0;
    int success_only = 0;
    const char *host = NULL;

    static struct option long_options[] = {
        {"success-only", no_argument, 0, 's'},
        {"mode", required_argument, 0, 'm'},
        {"range", required_argument, 0, 'r'},
        {"timeout", required_argument, 0, 't'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "m:r:t:s", long_options,
                              &option_index)) != -1) {

        switch (opt) {
        case 'm':
            if (mode != -1) {
                fprintf(stderr,
                        "Usage: %s [-m --mode mode] [-r --range start-end] [-t "
                        "--timeout timeout] "
                        "host\n",
                        argv[0]);
                exit(EXIT_FAILURE);
            } else if (strcmp(optarg, "connect") == 0) {
                mode = 0;
            } else if (strcmp(optarg, "stealth") == 0) {
                mode = 1;
            } else {
                fprintf(stderr, "Invalid mode, use either connect or stealth.");
                exit(EXIT_FAILURE);
            }
            break;
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
                    "Usage: %s [-m --mode mode] [-r --range start-end] [-t "
                    "--timeout timeout] [-s --success-only] "
                    "host\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Host argument is required.\n");
        fprintf(stderr,
                "Usage: %s [-m --mode mode] [-r --range start-end] [-t "
                "--timeout timeout] [-s --success-only] "
                "host\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    host = argv[optind];

    printf("Scanning %s from port %d to %d with a %.2fs timeout...\n", host,
           start_port, end_port, timeout);
    if (mode == 0) {
        connect_scanner(host, start_port, end_port, timeout, success_only);
    } else if (mode == 1) {
        stealth_scanner(host, start_port, end_port, timeout, success_only);
    } else {
        fprintf(stderr,
                "Usage: %s [-m --mode mode] [-r --range start-end] [-t "
                "--timeout timeout] [-s --success-only] "
                "host\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
