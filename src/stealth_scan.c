#include "stealth_scan.h"
#include "scan_utils.h"
#include <arpa/inet.h>
// #include <errno.h>
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
static atomic_int successes, timeouts, failures = 0;
static int scan_results[65535] = {0};

void *stealth_scanner_thread(void *arg) {
    int sockfd;
    stealth_scan_config *config = (stealth_scan_config *)arg;
    while (1) {
        int port = atomic_fetch_add(&current_port, 1);

        if (port > config->end_port) {
            break;
        }

        if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
            perror("Socket error");
            exit(EXIT_FAILURE);
        }

        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        // We set IP_HDRINCL to manually create the IP header
        int opt = 1;
        if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0) {
            perror("Setsockopt error");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        char packet[4096];
        memset(packet, 0, 4096);
        int total_packet_len = sizeof(struct iphdr) + sizeof(struct tcphdr);

        struct iphdr *ip_header = (struct iphdr *)packet;
        struct tcphdr *tcp_header =
            (struct tcphdr *)(packet + sizeof(struct iphdr));

        uint32_t source_addr = get_source_addr(config->host);
        uint32_t dest_addr;

        if (inet_pton(AF_INET, config->host, &dest_addr) <= 0) {
            fprintf(stderr, "%s is an invalid address\n", config->host);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // IP headers as directed by RFC 791
        ip_header->version = 4;
        ip_header->ihl = 5;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
        ip_header->id = htons(12345);
        ip_header->frag_off = 0;
        ip_header->ttl = 64;
        ip_header->protocol = IPPROTO_TCP;
        ip_header->check = 0; // Let the OS handle the IP checksum
        ip_header->saddr = source_addr;
        ip_header->daddr = dest_addr;

        // TCP headers as directed by RFC 9293
        tcp_header->source = htons(80);
        tcp_header->dest = htons(port);
        tcp_header->seq = 0;
        tcp_header->ack = 0;
        tcp_header->doff = 5;
        tcp_header->urg = 0;
        tcp_header->ack = 0;
        tcp_header->psh = 0;
        tcp_header->rst = 0;
        tcp_header->syn = 1;
        tcp_header->fin = 0;
        tcp_header->window = htons(5840);
        tcp_header->check = 0;

        // We have to create a pseudo header for the checksum
        int total_pseudo_len =
            sizeof(pseudo_tcp_header) + sizeof(struct tcphdr);
        char *checksum_buf = malloc(total_pseudo_len);

        pseudo_tcp_header pseudo_header;
        pseudo_header.source_address = source_addr;
        pseudo_header.dest_address = dest_addr;
        pseudo_header.placeholder = 0;
        pseudo_header.protocol = IPPROTO_TCP;
        pseudo_header.tcp_length = htons(sizeof(struct tcphdr));

        memcpy(checksum_buf, &pseudo_header, sizeof(pseudo_tcp_header));
        memcpy(checksum_buf + sizeof(pseudo_tcp_header), tcp_header,
               sizeof(struct tcphdr));
        tcp_header->check =
            csum((unsigned short *)checksum_buf, total_pseudo_len);
        free(checksum_buf);

        // sendto loop
        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = tcp_header->dest;
        servaddr.sin_addr.s_addr = dest_addr;

        if (sendto(sockfd, packet, total_packet_len, 0,
                   (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("Sendto failed");
            close(sockfd);
            scan_results[port] = 2;
            atomic_fetch_add(&failures, 1);
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
                scan_results[port] = 2;
                atomic_fetch_add(&timeouts, 1);
                break;
            } else if (select_result == 0) {
                scan_results[port] = 3;
                atomic_fetch_add(&timeouts, 1);
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
                        scan_results[port] = 1;
                        atomic_fetch_add(&successes, 1);
                    }

                    else if (recv_tcp->rst == 1) {
                        scan_results[port] = 2;
                        atomic_fetch_add(&failures, 1);
                    }
                    break;
                }
            }
        }
        close(sockfd);
    }
    return NULL;
}

void stealth_scanner(const char *host, int start_port, int end_port,
                     float timeout, int success_only) {
    atomic_init(&current_port, start_port);
    stealth_scan_config config = {
        .host = host, .end_port = end_port, .timeout = timeout};

    int number_of_threads = get_nprocs();
    pthread_t threads[number_of_threads];

    for (int i = 0; i < number_of_threads; i++) {
        pthread_create(&threads[i], NULL, stealth_scanner_thread, &config);
    }

    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int port = 0; port < end_port; port++) {
        if (scan_results[port] == 1) {
            printf("Connection successful on port %d\n", port);
        } else if (scan_results[port] == 2 && !success_only) {
            fprintf(stderr, "Connection failed on port %d\n", port);
        } else if (scan_results[port] == 3 && !success_only) {
            fprintf(stderr, "Connection timed out on port %d\n", port);
        }
    }
    printf("Successful connections: %d\n", successes);
    printf("Failed connections: %d\n", failures);
    printf("Timed out connections: %d\n", timeouts);
    printf("Scan complete\n");
}
