#ifndef FN_SCAN_H
#define FN_SCAN_H

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
    int mode;
} fn_scan_config;

void *fn_scanner_thread(void *arg);
void fn_scanner(const char *host, int start_port, int end_port, float timeout,
                int success_only, uint16_t *user_ports, size_t num_ports,
                int mode);

#endif
