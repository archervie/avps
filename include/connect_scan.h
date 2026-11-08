#ifndef CONNECT_SCAN_H
#define CONNECT_SCAN_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

void *connect_scanner_thread(void *arg);
void connect_scanner(const char *host, uint32_t start_port, uint32_t end_port,
                     float timeout, int success_only, uint16_t *user_ports,
                     size_t num_ports);

#endif
