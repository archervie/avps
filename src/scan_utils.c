#include "scan_utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint16_t *parse_ports(char *port_str, size_t *out_count) {
    size_t capacity = 10;
    size_t count = 0;

    uint16_t *ports = malloc(capacity * sizeof(uint16_t));
    if (!ports)
        return NULL;

    char *token = strtok(port_str, ",");
    while (token != NULL) {
        if (count >= capacity) {
            capacity *= 2;
            ports = realloc(ports, capacity * sizeof(uint16_t));
            if (!ports)
                return NULL;
        }

        int port = strtol(token, NULL, 0);
        if (port > 0 && port <= 65535) {
            ports[count++] = (uint16_t)port;
        }

        token = strtok(NULL, ",");
    }

    *out_count = count;
    return ports;
}

uint32_t get_source_addr(const char *host) {
    // We create a simple datagram socket to get
    // the source address of the client machine
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return 0;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(80);

    if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
        close(sockfd);
        return 0;
    }

    int status =
        connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (status < 0) {
        close(sockfd);
        return 0;
    }

    struct sockaddr_in name;
    socklen_t name_len = sizeof(name);
    if (getsockname(sockfd, (struct sockaddr *)&name, &name_len) < 0) {
        close(sockfd);
        return 0;
    }

    uint32_t source_addr = name.sin_addr.s_addr;
    close(sockfd);
    return source_addr;
}

unsigned short csum(unsigned short *checksum_buf, int nbytes) {
    long sum;
    unsigned short odd_byte;
    short checksum;

    // Creates a sum of all the 2-byte words
    sum = 0;
    while (nbytes > 1) {
        sum += *checksum_buf++;
        nbytes -= 2;
    }

    // If there's an odd number of bytes, we add
    // a 0 to the end of sum
    if (nbytes == 1) {
        odd_byte = 0;
        *((u_char *)&odd_byte) = *(u_char *)checksum_buf;
        sum += odd_byte;
    }

    // We carry over the overflow values to the end of
    // the sum
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);

    // We invert the bits of the sum
    checksum = (short)~sum;
    return checksum;
}
