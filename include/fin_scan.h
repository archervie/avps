#ifndef FIN_SCAN_H
#define FIN_SCAN_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *host;
    int end_port;
    float timeout;
    uint16_t *user_ports;
    size_t num_ports;
    atomic_size_t current_index;
} fn_scan_config;

void *fin_scanner_thread(void *arg);
void fin_scanner(const char *host, uint32_t start_port, uint32_t end_port,
                 float timeout, int success_only, uint16_t *user_ports,
                 size_t num_ports);

#endif
