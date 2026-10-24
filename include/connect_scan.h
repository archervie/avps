#ifndef CONNECT_SCAN_H
#define CONNECT_SCAN_H

typedef struct {
    const char *host;
    int end_port;
    float timeout;
} connect_scan_config;

void *connect_scanner_thread(void *arg);
void connect_scanner(const char *host, int start_port, int end_port,
                     float timeout, int success_only);

#endif
