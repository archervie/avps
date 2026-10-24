#ifndef SCAN_H
#define SCAN_H

typedef struct {
    const char *host;
    int end_port;
    float timeout;
} stealth_scan_config;

void *stealth_scanner_thread(void *arg);
void stealth_scanner(const char *host, int start_port, int end_port,
                     float timeout, int success_only);

#endif
