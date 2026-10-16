#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {

    int opt;
    int option_index = 0;

    int start_port = 1;
    int end_port = 65535;
    float timeout = 1.0;
    const char *host = NULL;

    static struct option long_options[] = {
        {"range", required_argument, 0, 'r'},
        {"timeout", required_argument, 0, 't'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "r:t:", long_options,
                              &option_index)) != -1) {

        switch (opt) {
        case 'r':
            if (sscanf(optarg, "%d-%d", &start_port, &end_port) != 2) {
                fprintf(stderr,
                        "Invalid range, use start-end (e.g. 1-1000).\n");
            }

        case 't':
            timeout = atof(optarg);
        }
    }

    int sockfd, status;
    struct sockaddr_in servaddr;

    if (argc != 2) {
        fprintf(stderr, "Usage: avps [host]\n");
        exit(EXIT_FAILURE);
    } else {
        host = argv[1];
    }

    for (int port = 1; port < 65535; port++) {
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "Socket failed to be created\n");
            exit(EXIT_FAILURE);
        }

        // Non-blocking mode
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        memset(&servaddr, 0, sizeof(servaddr));

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);

        if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
            fprintf(stderr, "%s is an invalid address\n", argv[1]);
            exit(EXIT_FAILURE);
        }

        int status =
            connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

        if (status == -1 && errno == EINPROGRESS) {
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(sockfd, &fdset);

            int select_result = select(sockfd + 1, NULL, &fdset, NULL, &tv);
            if (select_result == 1) {
                int so_err;
                socklen_t len = sizeof(so_err);
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_err, &len);

                if (so_err == 0) {
                    fprintf(stdout, "Connection successful on port %d\n", port);
                } else {
                    fprintf(stderr, "Connection failed on port %d\n", port);
                }
            } else if (select_result == 0) {
                fprintf(stderr, "Connection timed out on port %d\n", port);
            } else {
                fprintf(stderr, "Select error on port %d\n", port);
            }
        } else if (status == 0) {
            fprintf(stdout, "Connection successful on port %d\n", port);
        }
        close(sockfd);
    }
}
