#include "scan_utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

void *receiver_thread(void *arg) {
    scan_config *config = (scan_config *)arg;
    uint32_t target_addr;

    if (inet_pton(AF_INET, config->host, &target_addr) <= 0) {
        // Sender thread already verifies this so to avoid double
        // print we just return NULL here
        return NULL;
    }

    int tcp_sock = create_recv_socket(IPPROTO_TCP);
    if (tcp_sock < 0) {
        return NULL;
    }

    int icmp_sock = create_recv_socket(IPPROTO_ICMP);
    if (icmp_sock < 0) {
        close(tcp_sock);
        return NULL;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Epoll_create1 failed");
        close(tcp_sock);
        close(icmp_sock);
        return NULL;
    }

    struct epoll_event ev, events[10];
    ev.events = EPOLLIN;

    ev.data.fd = tcp_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_sock, &ev) == -1) {
        perror("Epoll_ctl (TCP) failed");
        close(epoll_fd);
        close(tcp_sock);
        close(icmp_sock);
        return NULL;
    }

    ev.data.fd = icmp_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, icmp_sock, &ev) == -1) {
        perror("Epoll_ctl (ICMP) failed");
        close(epoll_fd);
        close(tcp_sock);
        close(icmp_sock);
        return NULL;
    }

    char recv_buffer[4096];
    int timeout_ms = (int)(config->timeout * 1000);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, 10, timeout_ms);

        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait error");
            break;
        }

        if (nfds == 0) {
            if (atomic_load(&sender_done)) {
                break;
            } else {
                continue;
            }
        }

        for (int n = 0; n < nfds; ++n) {
            int active_fd = events[n].data.fd;

            // Drain all queued packets from the ready socket
            while (1) {
                int bytes_received = recvfrom(
                    active_fd, recv_buffer, sizeof(recv_buffer), 0, NULL, NULL);

                if (bytes_received < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break; // Socket buffer is drained
                    }
                    perror("recvfrom error");
                    break;
                }

                if ((size_t)bytes_received < sizeof(struct iphdr)) {
                    continue;
                }

                struct iphdr *recv_ip = (struct iphdr *)recv_buffer;
                int ip_header_len = recv_ip->ihl * 4;

                if (active_fd == tcp_sock) {
                    if (recv_ip->saddr != target_addr) {
                        continue;
                    }

                    if ((size_t)bytes_received <
                        (size_t)ip_header_len + sizeof(struct tcphdr)) {
                        continue;
                    }

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
                        if (recv_tcp->rst == 1) {
                            scan_results[scanned_port] = 2;
                        }
                        break;
                    }
                } else if (active_fd == icmp_sock &&
                           (config->mode == 2 || config->mode == 3)) {
                    // Min size: Outer IP + ICMP(8) + Inner IP(20) + Inner
                    // TCP(8)
                    if ((size_t)bytes_received < (size_t)ip_header_len +
                                                     sizeof(struct icmphdr) +
                                                     sizeof(struct iphdr) + 8) {
                        continue;
                    }

                    struct icmphdr *icmp =
                        (struct icmphdr *)(recv_buffer + ip_header_len);

                    if (icmp->type == ICMP_DEST_UNREACH) {
                        struct iphdr *orig_ip =
                            (struct iphdr *)(recv_buffer + ip_header_len +
                                             sizeof(struct icmphdr));

                        // Verify the ICMP error is for our target host
                        if (orig_ip->daddr != target_addr) {
                            continue;
                        }

                        int orig_ip_header_len = orig_ip->ihl * 4;

                        if ((size_t)bytes_received <
                            (size_t)ip_header_len + sizeof(struct icmphdr) +
                                (size_t)orig_ip_header_len + 8) {
                            continue;
                        }

                        struct tcphdr *orig_tcp =
                            (struct tcphdr *)((unsigned char *)orig_ip +
                                              orig_ip_header_len);

                        uint16_t original_scanned_port = ntohs(orig_tcp->dest);
                        scan_results[original_scanned_port] = 3;
                    }
                }
            }
        }
    }

    close(epoll_fd);
    close(tcp_sock);
    close(icmp_sock);
    return NULL;
}
