#ifndef SCAN_H
#define SCAN_H

typedef struct {
    char *addr;
    const char *host;
    int end_port;
    float timeout;
} scan_config;

void *scan_thread(void *arg);
void connect_scanner(char *addr, const char *host, int start_port, int end_port,
             float timeout, int success_only);

#endif
