#include "core_utils.h"
#include "scan_utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *sender_thread(void *arg) {
    scan_config *config = (scan_config *)arg;
    uint32_t source_addr = get_source_addr(config->host);
    uint32_t dest_addr;

    if (inet_pton(AF_INET, config->host, &dest_addr) <= 0) {
        fprintf(stderr, "%s is an invalid address\n", config->host);
        exit(EXIT_FAILURE);
    }

    int sockfd = create_raw_socket();

    char packet[4096];
    int total_packet_len = sizeof(struct iphdr) + sizeof(struct tcphdr);

    while (1) {
        uint32_t dest_port;

        if (config->num_ports > 0) {
            size_t port_idx = atomic_fetch_add(&current_idx, 1);

            if (port_idx >= config->num_ports) {
                break;
            }

            dest_port = config->user_ports[port_idx];
        } else {
            dest_port = atomic_fetch_add(&current_port, 1);

            if (dest_port > config->end_port) {
                break;
            }
        }

        memset(packet, 0, 4096);

        struct iphdr *ip_header = (struct iphdr *)packet;
        struct tcphdr *tcp_header =
            (struct tcphdr *)(packet + sizeof(struct iphdr));

        build_ip_header(ip_header, source_addr, dest_addr);
        build_tcp_header(tcp_header, dest_port);

        tcp_header->syn = 1;

        calculate_tcp_checksum(tcp_header, source_addr, dest_addr);

        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = tcp_header->dest;
        servaddr.sin_addr.s_addr = dest_addr;

        int sent = 0;
        while (!sent) {
            if (sendto(sockfd, packet, total_packet_len, 0,
                       (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {

                if (errno == EAGAIN || errno == EWOULDBLOCK ||
                    errno == ENOBUFS) {
                    usleep(1000);
                } else {
                    perror("Sendto failed");
                    break;
                }
            } else {
                sent = 1;
            }
        }

        // Add a tiny baseline delay (e.g., 500 microseconds) to rate-limit the
        // scanner and prevent it from constantly hitting the buffer limit.
        usleep(500);
    }
    close(sockfd);
    return NULL;
}
