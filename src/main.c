#include "connect_scan.h"
#include "fin_scan.h"
#include "null_scan.h"
#include "syn_scan.h"
#include "utils.h"
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_err_msg(char *name);

int main(int argc, char **argv) {
    int opt;
    int option_index = 0;
    int mode = 0;
    int start_port = 1;
    int end_port = 65535;
    float timeout = 1.0;
    int success_only = 0;
    uint16_t num_ports = 0;
    uint16_t *user_ports = NULL;
    const char *host = NULL;

    static struct option long_options[] = {
        {"success-only", no_argument, 0, 's'},
        {"known-ports", required_argument, 0, 'k'},
        {"mode", required_argument, 0, 'm'},
        {"range", required_argument, 0, 'r'},
        {"timeout", required_argument, 0, 't'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "k:m:r:t:s", long_options,
                              &option_index)) != -1) {

        switch (opt) {
        case 'm':
            if (strcmp(optarg, "connect") == 0) {
                mode = 0;
            } else if (strcmp(optarg, "syn") == 0) {
                mode = 1;
            } else if (strcmp(optarg, "fin") == 0) {
                mode = 2;
            } else if (strcmp(optarg, "null") == 0) {
                mode = 3;
            } else {
                fprintf(stderr,
                        "Invalid mode, use either connect, syn, fin or null. "
                        "Default is connect");
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

        case 'k':
            user_ports = parse_ports(optarg, &num_ports);
            if (!user_ports) {
                fprintf(stderr, "Invalid list, use a comma-separated list "
                                "(e.g. 22,80,443)\n");
                exit(EXIT_FAILURE);
            }
            break;

        default:
            print_err_msg(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Host argument is required.\n");
        print_err_msg(argv[0]);
        exit(EXIT_FAILURE);
    }
    host = argv[optind];

    if (!num_ports) {
        printf("Scanning %s from port %d to %d with a %.2fs timeout...\n", host,
               start_port, end_port, timeout);
    } else if (num_ports == 1) {
        printf("Scanning %s through %d port with a %.2fs timeout...\n", host,
               (int)num_ports, timeout);
    } else {
        printf("Scanning %s through %d ports with a %.2fs timeout...\n", host,
               (int)num_ports, timeout);
    }

    switch (mode) {
    case 0:
        connect_scanner(host, start_port, end_port, timeout, success_only,
                        user_ports, num_ports);
        break;
    case 1:
        syn_scanner(host, start_port, end_port, timeout, success_only,
                    user_ports, num_ports);
        break;
    case 2:
        fin_scanner(host, start_port, end_port, timeout, success_only,
                    user_ports, num_ports);
        break;
    case 3:
        null_scanner(host, start_port, end_port, timeout, success_only,
                     user_ports, num_ports);
        break;
    default:
        print_err_msg(argv[0]);
        exit(EXIT_FAILURE);
    }

    free(user_ports);
    exit(EXIT_SUCCESS);
}

void print_err_msg(char *name) {
    fprintf(stderr,
            "Usage: %s [-m --mode mode] [-r --range start-end] [-t "
            "--timeout timeout] [-s --success-only] [-k --known-ports] "
            "host\n",
            name);
}
