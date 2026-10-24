#ifndef SCAN_UTILS_H
#define SCAN_UTILS_H

#include <stdint.h>
#include <unistd.h>

typedef struct {
    uint32_t source_address;
    uint32_t dest_address;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t tcp_length;
} pseudo_tcp_header;

uint32_t get_source_addr(const char *host);
unsigned short csum(unsigned short *ptr, int nbytes);
uint16_t *parse_ports(char *port_str, size_t *out_count);

#endif
