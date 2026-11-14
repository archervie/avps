#ifndef SYN_SCAN_H
#define SYN_SCAN_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

void *syn_scanner_thread(void *arg);
void syn_scanner(const char *host, uint32_t start_port, uint32_t end_port,
                 float timeout, int success_only, uint16_t *user_ports,
                 size_t num_ports);

#endif
