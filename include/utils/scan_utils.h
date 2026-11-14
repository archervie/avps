#ifndef SCAN_UTILS_H
#define SCAN_UTILS_H

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    const char *host;
    uint32_t end_port;
    float timeout;
    uint16_t *user_ports;
    size_t num_ports;
} scan_config;

int create_raw_socket();
void build_ip_header(struct iphdr *ip_header, uint32_t source_addr,
                     uint32_t dest_addr);
void build_tcp_header(struct tcphdr *tcp_header, uint32_t dest_port);
void calculate_tcp_checksum(struct tcphdr *tcp_header, uint32_t source_addr,
                            uint32_t dest_addr);

#endif
