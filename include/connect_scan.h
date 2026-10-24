#ifndef CONNECT_SCAN_H
#define CONNECT_SCAN_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *host;
    int end_port;
    float timeout;
    uint16_t *user_ports;
    size_t num_ports;
} connect_scan_config;

void *connect_scanner_thread(void *arg);
void connect_scanner(const char *host, int start_port, int end_port,
                     float timeout, int success_only, uint16_t *user_ports,
                     size_t num_ports);

#endif
