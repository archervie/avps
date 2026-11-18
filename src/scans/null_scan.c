#include "null_scan.h"
#include "receiver.h"
#include "scan_utils.h"
#include "sender.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <unistd.h>

void null_scanner(const char *host, uint32_t start_port, uint32_t end_port,
                  float timeout, int success_only, uint16_t *user_ports,
                  size_t num_ports, int mode) {
    if (!user_ports) {
        atomic_init(&current_port, start_port);
    } else {
        atomic_init(&current_idx, 0);
    }

    scan_config config = {.host = host,
                          .end_port = end_port,
                          .timeout = timeout,
                          .user_ports = user_ports,
                          .num_ports = num_ports,
                          .mode = mode};

    pthread_t threads[2];

    pthread_create(&threads[0], NULL, sender_thread, &config);
    pthread_create(&threads[1], NULL, receiver_thread, &config);

    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }

    if (!num_ports) {
        for (uint32_t port = 1; port <= end_port; port++) {
            if (scan_results[port] == 0) {
                printf("No RST received on port %d\n", port);
                successes++;
            } else if (scan_results[port] == 2) {
                failures++;
                if (!success_only) {
                    fprintf(stderr, "RST received on port %d\n", port);
                }
            } else if (scan_results[port] == 3) {
                filtered++;
                if (!success_only) {
                    fprintf(stderr, "ICMP received on port %d; filtered\n",
                            port);
                }
            }
        }

    } else {
        for (int i = 0; i < (int)num_ports; i++) {
            int port = user_ports[i];
            if (scan_results[port] == 0) {
                printf("No RST received on port %d\n", port);
                successes++;
            } else if (scan_results[port] == 2) {
                failures++;
                if (!success_only) {
                    fprintf(stderr, "RST received on port %d\n", port);
                }
            } else if (scan_results[port] == 3) {
                filtered++;
                if (!success_only) {
                    fprintf(stderr, "ICMP received on port %d; filtered\n",
                            port);
                }
            }
        }
    }

    printf("Open/Filtered ports: %d\n", successes);
    printf("Closed ports: %d\n", failures);
    printf("Scan complete\n");
}
