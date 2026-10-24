#include "connect_scan.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <unistd.h>

static atomic_int current_port;
static atomic_int successes, timeouts, failures = 0;
static int scan_results[65535] = {0};

void *connect_scanner_thread(void *arg) {
    int sockfd;
    connect_scan_config *config = (connect_scan_config *)arg;
    struct sockaddr_in servaddr;

    while (1) {
        int port = atomic_fetch_add(&current_port, 1);

        struct timeval tv;
        tv.tv_sec = (int)(config->timeout);
        tv.tv_usec = (int)((config->timeout - tv.tv_sec) * 1000000);

        if (port > config->end_port) {
            break;
        }

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "Socket failed to be created\n");
            exit(EXIT_FAILURE);
        }

        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        memset(&servaddr, 0, sizeof(servaddr));

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);

        if (inet_pton(AF_INET, config->host, &servaddr.sin_addr) <= 0) {
            fprintf(stderr, "%s is an invalid address\n", config->host);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        int status =
            connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

        if (status == -1 && errno == EINPROGRESS) {
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(sockfd, &fdset);

            int select_result = select(sockfd + 1, NULL, &fdset, NULL, &tv);
            if (select_result == 1) {
                int so_err;
                socklen_t len = sizeof(so_err);
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_err, &len);

                if (so_err == 0) {
                    scan_results[port] = 1;
                    atomic_fetch_add(&successes, 1);
                } else {
                    scan_results[port] = 2;
                    atomic_fetch_add(&failures, 1);
                }
            } else if (select_result == 0) {
                scan_results[port] = 3;
                atomic_fetch_add(&timeouts, 1);
            } else {
                scan_results[port] = 2;
                atomic_fetch_add(&failures, 1);
            }
        } else if (status == 0) {
            scan_results[port] = 2;
            atomic_fetch_add(&successes, 1);
        }
        close(sockfd);
    }
    return NULL;
}

void connect_scanner(const char *host, int start_port, int end_port,
                     float timeout, int success_only) {
    atomic_init(&current_port, start_port);
    connect_scan_config config = {
        .host = host, .end_port = end_port, .timeout = timeout};

    int number_of_threads = get_nprocs();
    pthread_t threads[number_of_threads];

    for (int i = 0; i < number_of_threads; i++) {
        pthread_create(&threads[i], NULL, connect_scanner_thread, &config);
    }

    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int port = 0; port < end_port; port++) {
        if (scan_results[port] == 1) {
            printf("Connection successful on port %d\n", port);
        } else if (scan_results[port] == 2 && !success_only) {
            fprintf(stderr, "Connection failed on port %d\n", port);
        } else if (scan_results[port] == 3 && !success_only) {
            fprintf(stderr, "Connection timed out on port %d\n", port);
        }
    }
    printf("Successful connections: %d\n", successes);
    printf("Failed connections: %d\n", failures);
    printf("Timed out connections: %d\n", timeouts);
    printf("Scan complete\n");
}
