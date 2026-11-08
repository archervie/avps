#include "utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int create_raw_socket(void) {
    // This function creates a proper raw socket to be used
    // in the scan. It is a non-blocking socket with IP_HDRINCL
    // set to allow us to manually construct the IP header
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) < 0) {
        perror("Socket error");
        exit(EXIT_FAILURE);
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    int opt = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt error");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void build_ip_header(struct iphdr *ip_header, uint32_t source_addr,
                     uint32_t dest_addr) {
    // This function constructs the IP header. This
    // stays uniform throughout all scans. These are as directed
    // by RFC 791
    // NOTE: We let the OS handle the creation of the IP checksum
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
}

void build_tcp_header(struct tcphdr *tcp_header, int dest_port) {
    // This function constructs some parts of the TCP header. The
    // exact flags change from scan to scan, so not all flags are
    // set. These are as directed by RFC 9293
    tcp_header->source = htons(80);
    tcp_header->dest = htons(dest_port);
    tcp_header->seq = 0;
    tcp_header->doff = 5;
    tcp_header->window = htons(5840);
    tcp_header->check = 0;
}

void calculate_tcp_checksum(struct tcphdr *tcp_header, uint32_t source_addr,
                            uint32_t dest_addr) {
    // This function creates a pseudo TCP header to use during
    // the TCP checksum calculation as defined in utils.c
    int total_pseudo_len = sizeof(pseudo_tcp_header) + sizeof(struct tcphdr);
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

    tcp_header->check = csum((unsigned short *)checksum_buf, total_pseudo_len);
    free(checksum_buf);
}
