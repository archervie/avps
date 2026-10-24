#include "scan_utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

uint32_t get_source_addr(const char *host) {
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return 0;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
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
    socklen_t namelen = sizeof(name);
    if (getsockname(sockfd, (struct sockaddr *)&name, &namelen) < 0) {
        close(sockfd);
        return 0;
    }

    uint32_t source_addr = name.sin_addr.s_addr;
    close(sockfd);
    return source_addr;
}

unsigned short csum(unsigned short *ptr, int nbytes) {
    long sum;
    unsigned short oddbyte;
    short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char *)&oddbyte) = *(u_char *)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
    return answer;
}
