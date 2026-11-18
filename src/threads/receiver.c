#include "scan_utils.h"
#include <arpa/inet.h>
#include <linux/filter.h>
#include <netinet/ip_icmp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>

void *receiver_thread(void *arg) {
    scan_config *config = (scan_config *)arg;
    uint32_t target_addr;

    if (inet_pton(AF_INET, config->host, &target_addr) <= 0) {
        // Sender thread already verifies this so to avoid double
        // print we just return NULL here
        return NULL;
    }

    int sockfd = create_raw_socket();

    /*
     * - Standard BPF to filter for TCP (6) and ICMP (1)
     * - sock_filter layout: { code, jt, jf, k }
     */
    struct sock_filter bpf_code[] = {
        {0x30, 0, 0, 0x00000009}, // ldb [9] - Load IPv4 protocol byte
        {0x15, 2, 0,
         0x00000006}, // jeq #6  - If TCP (6), jump 2 instructions to ACCEPT
        {0x15, 1, 0,
         0x00000001}, // jeq #1  - If ICMP (1), jump 1 instruction to ACCEPT
        {0x06, 0, 0, 0x00000000}, // ret 0   - REJECT packet (fallthrough)
        {0x06, 0, 0, 0xffffffff}  // ret -1  - ACCEPT whole packet
    };

    struct sock_fprog bpf = {
        .len = sizeof(bpf_code) /
               sizeof(bpf_code[0]), // Dynamically calculate length
        .filter = bpf_code,
    };

    if (setsockopt(sockfd, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)) <
        0) {
        perror("Setsockopt BPF failed");
        close(sockfd);
        return NULL;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Epoll_create1 failed");
        close(sockfd);
        return NULL;
    }

    struct epoll_event ev, events[10];
    ev.events = EPOLLIN; // We only care about incoming data
    ev.data.fd = sockfd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        perror("Epoll_ctl failed");
        close(epoll_fd);
        close(sockfd);
        return NULL;
    }

    char recv_buffer[4096];

    int timeout_ms = (int)(config->timeout * 1000);

    while (1) {
        // Wait for incoming packets; if timeout is reached with no packets,
        // nfds will be 0, allowing the thread to exit gracefully.
        int nfds = epoll_wait(epoll_fd, events, 10, timeout_ms);

        if (nfds == -1) {
            perror("epoll_wait error");
            break;
        }

        // If the sender thread is done and no packets arrive within the
        // timeout, we assume the scan is complete and exit the receiver thread.
        if (nfds == 0) {
            break;
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == sockfd) {
                int bytes_received = recvfrom(
                    sockfd, recv_buffer, sizeof(recv_buffer), 0, NULL, NULL);
                if (bytes_received < 0) {
                    continue;
                }

                struct iphdr *recv_ip = (struct iphdr *)recv_buffer;

                if (recv_ip->saddr != target_addr) {
                    continue;
                }

                // Calculate where the TCP header starts
                int ip_header_len = recv_ip->ihl * 4;
                struct tcphdr *recv_tcp =
                    (struct tcphdr *)(recv_buffer + ip_header_len);

                uint16_t scanned_port = ntohs(recv_tcp->source);

                switch (config->mode) {
                case 1:
                    if (recv_tcp->syn == 1 && recv_tcp->ack == 1) {
                        scan_results[scanned_port] = 1;
                    } else if (recv_tcp->rst == 1) {
                        scan_results[scanned_port] = 2;
                    }
                    break;

                case 2:
                case 3:
                    if (recv_ip->protocol == IPPROTO_TCP) {
                        int ip_header_len = recv_ip->ihl * 4;
                        struct tcphdr *recv_tcp =
                            (struct tcphdr *)(recv_buffer + ip_header_len);
                        uint16_t scanned_port = ntohs(recv_tcp->source);

                        if (recv_tcp->rst == 1) {
                            scan_results[scanned_port] = 2;
                        }
                    } else if (recv_ip->protocol == IPPROTO_ICMP) {
                        int ip_header_len = recv_ip->ihl * 4;

                        // Make sure we captured enough data to parse the nested
                        // headers safely Min size: Outer IP(20) + ICMP(8) +
                        // Inner IP(20) + Inner TCP(8) = 56 bytes
                        if ((size_t)bytes_received <
                            (size_t)ip_header_len + sizeof(struct icmphdr) +
                                sizeof(struct iphdr) + 8) {
                            continue; // Packet too short, ignore
                        }

                        struct icmphdr *icmp =
                            (struct icmphdr *)(recv_buffer + ip_header_len);

                        if (icmp->type == ICMP_DEST_UNREACH) {

                            struct iphdr *orig_ip =
                                (struct iphdr *)(recv_buffer + ip_header_len +
                                                 sizeof(struct icmphdr));

                            int orig_ip_header_len = orig_ip->ihl * 4;

                            struct tcphdr *orig_tcp =
                                (struct tcphdr *)((unsigned char *)orig_ip +
                                                  orig_ip_header_len);

                            uint16_t original_scanned_port =
                                ntohs(orig_tcp->dest);

                            scan_results[original_scanned_port] = 3;
                        }
                        break;
                    }
                }
            }
        }
    }
    close(epoll_fd);
    close(sockfd);
    return NULL;
}
