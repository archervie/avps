#include "stealth_scan.h"
#include "scan_utils.h"
#include <arpa/inet.h>
#include <errno.h>
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
// static atomic_int successes, timeouts, failures = 0;
// static int scan_results[65535] = {0};

void *stealth_scan_thread(void *arg) {
    int sockfd;
    stealth_scan_config *config = (stealth_scan_config *)arg;
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
        perror("Socket error");
        exit(EXIT_FAILURE);
    }

    // We set IP_HDRINCL to manually create the IP header
    int opt = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt error");
        exit(EXIT_FAILURE);
    }

    char packet[4096];
    memset(packet, 0, 4096);

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
    tcp_header->dest = htons(current_port);
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
    int total_len = sizeof(pseudo_tcp_header) + sizeof(struct tcphdr);
    char *checksum_buf = malloc(total_len);

    pseudo_tcp_header pseudo_header;
    pseudo_header.source_address = source_addr;
    pseudo_header.dest_address = dest_addr;
    pseudo_header.placeholder = 0;
    pseudo_header.protocol = IPPROTO_TCP;
    pseudo_header.tcp_length = htons(sizeof(struct tcphdr));

    memcpy(checksum_buf, &pseudo_header, sizeof(pseudo_tcp_header));
    memcpy(checksum_buf + sizeof(pseudo_tcp_header), &tcp_header,
           sizeof(struct tcphdr));
    tcp_header->check = csum((unsigned short *)checksum_buf, total_len);
    free(checksum_buf);
    return NULL;
}

// void stealth_scanner(const char *host, int start_port, int end_port,
//                      float timeout, int success_only) {
//     atomic_init(&current_port, start_port);
// }
