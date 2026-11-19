#include "syn_scan.h"
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

void syn_scanner(const char *host, uint32_t start_port, uint32_t end_port,
                 float timeout, int success_only, uint16_t *user_ports,
                 size_t num_ports, int mode) {
    if (!user_ports) {
        atomic_init(&current_port, start_port);
        for (uint32_t p = start_port; p <= end_port; p++) {
            scan_results[p] = 4;
        }
    } else {
        atomic_init(&current_idx, 0);
        for (size_t i = 0; i < num_ports; i++) {
            scan_results[user_ports[i]] = 4;
        }
    }

    scan_config config = {.host = host,
                          .end_port = end_port,
                          .timeout = timeout,
                          .user_ports = user_ports,
                          .num_ports = num_ports,
                          .mode = mode};

    pthread_t threads[2];

    pthread_create(&threads[0], NULL, receiver_thread, &config);
    pthread_create(&threads[1], NULL, sender_thread, &config);

    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }

    if (!num_ports) {
        for (uint32_t port = 1; port <= end_port; port++) {
            if (scan_results[port] == 1) {
                printf("SYN-ACK received on port %d\n", port);
                successes++;
            } else if (scan_results[port] == 2) {
                failures++;
                if (!success_only) {
                    fprintf(stderr, "RST received on port %d\n", port);
                }
            } else if (scan_results[port] == 4) {
                timeouts++;
                if (!success_only) {
                    fprintf(stderr, "Connection timed out on port %d\n", port);
                }
            }
        }
    } else {
        for (int i = 0; i < (int)num_ports; i++) {
            int port = user_ports[i];
            if (scan_results[port] == 1) {
                printf("SYN-ACK received on port %d\n", port);
                successes++;
            } else if (scan_results[port] == 2) {
                failures++;
                if (!success_only) {
                    fprintf(stderr, "RST received on port %d\n", port);
                }
            } else if (scan_results[port] == 4) {
                timeouts++;
                if (!success_only) {
                    fprintf(stderr, "Connection timed out on port %d\n", port);
                }
            }
        }
    }

    printf("Successful responses: %d\n", successes);
    printf("Failed responses: %d\n", failures);
    printf("Timed out connections: %d\n", timeouts);
    printf("Scan complete\n");
}
