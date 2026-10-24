#include "scan_utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

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
