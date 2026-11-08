#include "syn_scan.h"
#include "scan_utils.h"
#include "utils.h"
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

static atomic_int current_port;
static atomic_size_t current_idx;
static int successes, timeouts, failures = 0;
static int scan_results[65535] = {0};

void *syn_scanner_thread(void *arg) {
    scan_config *config = (scan_config *)arg;
    uint32_t source_addr = get_source_addr(config->host);
    uint32_t dest_addr;

    if (inet_pton(AF_INET, config->host, &dest_addr) <= 0) {
        fprintf(stderr, "%s is an invalid address\n", config->host);
        exit(EXIT_FAILURE);
    }

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

        int sockfd = create_raw_socket();

        char packet[4096];
        memset(packet, 0, 4096);
        int total_packet_len = sizeof(struct iphdr) + sizeof(struct tcphdr);

        struct iphdr *ip_header = (struct iphdr *)packet;
        struct tcphdr *tcp_header =
            (struct tcphdr *)(packet + sizeof(struct iphdr));

        build_ip_header(ip_header, source_addr, dest_addr);
        build_tcp_header(tcp_header, dest_port);

        tcp_header->ack = 0;
        tcp_header->urg = 0;
        tcp_header->ack = 0;
        tcp_header->psh = 0;
        tcp_header->rst = 0;
        tcp_header->syn = 1;
        tcp_header->fin = 0;

        // We have to create a pseudo header for the checksum
        calculate_tcp_checksum(tcp_header, source_addr, dest_addr);

        // sendto loop
        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = tcp_header->dest;
        servaddr.sin_addr.s_addr = dest_addr;

        if (sendto(sockfd, packet, total_packet_len, 0,
                   (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("Sendto failed");
            close(sockfd);
            scan_results[dest_port] = 2;
            continue;
        }

        fd_set fdset;
        struct timeval tv;
        char recv_buffer[4096]; // Buffer for incoming packets

        while (1) {
            FD_ZERO(&fdset);
            FD_SET(sockfd, &fdset);

            tv.tv_sec = (int)config->timeout;
            tv.tv_usec = (int)(config->timeout - tv.tv_sec) * 1000000;

            int select_result = select(sockfd + 1, &fdset, NULL, NULL, &tv);

            if (select_result < 0) {
                perror("Select error");
                scan_results[dest_port] = 2;
                break;
            } else if (select_result == 0) {
                scan_results[dest_port] = 3;
                break;
            }

            if (FD_ISSET(sockfd, &fdset)) {
                int bytes_received = recvfrom(
                    sockfd, recv_buffer, sizeof(recv_buffer), 0, NULL, NULL);
                if (bytes_received < 0) {
                    perror("Recvfrom error");
                    continue;
                }

                struct iphdr *recv_ip = (struct iphdr *)recv_buffer;
                int ip_header_len = recv_ip->ihl * 4;
                struct tcphdr *recv_tcp =
                    (struct tcphdr *)(recv_buffer + ip_header_len);

                if (recv_tcp->source == tcp_header->dest &&
                    recv_tcp->dest == tcp_header->source) {

                    if (recv_tcp->syn == 1 && recv_tcp->ack == 1) {
                        scan_results[dest_port] = 1;
                    }

                    else if (recv_tcp->rst == 1) {
                        scan_results[dest_port] = 2;
                    }
                    break;
                }
            }
        }
        close(sockfd);
    }
    return NULL;
}

void syn_scanner(const char *host, uint32_t start_port, uint32_t end_port,
                 float timeout, int success_only, uint16_t *user_ports,
                 size_t num_ports) {
    if (!user_ports) {
        atomic_init(&current_port, start_port);
    } else {
        atomic_init(&current_idx, 0);
    }

    scan_config config = {.host = host,
                          .end_port = end_port,
                          .timeout = timeout,
                          .user_ports = user_ports,
                          .num_ports = num_ports};

    int number_of_threads = get_nprocs();
    pthread_t threads[number_of_threads];

    for (int i = 0; i < number_of_threads; i++) {
        pthread_create(&threads[i], NULL, syn_scanner_thread, &config);
    }

    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    if (!num_ports) {
        for (uint32_t port = 0; port <= end_port; port++) {
            if (scan_results[port] == 1) {
                printf("SYN-ACK received on port %d\n", port);
                successes++;
            } else if (scan_results[port] == 2) {
                failures++;
                if (!success_only) {
                    fprintf(stderr, "RST received on port %d\n", port);
                }
            } else if (scan_results[port] == 3) {
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
            } else if (scan_results[port] == 3) {
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
