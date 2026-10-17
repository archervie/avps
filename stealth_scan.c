#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>

atomic_int current_port;
atomic_int successes, timeouts, failures = 0;
int scan_results[65535] = {0};

void *stealth_scan_thread(void *arg) {
   int sockfd;
   if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
       fprintf(stderr, "Socket failed to be created\n");
       exit(EXIT_FAILURE);
   }

   int opt = 1;
   if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0) {
       perror("Error setting IP_HDRINCL");
       exit(EXIT_FAILURE);
   }

   char packet[4096];
   memset(packet, 0, 4096);

   struct iphdr *ip_header = (struct iphdr *)packet;
   struct tcphdr *tcp_header = (struct tcphdr *)(packet + sizeof(struct iphdr));

   struct ifaddrs *ifap, *ifa;
   char ip_str[INET_ADDRSTRLEN];

   

   ip_header->version = 4;
   ip_header->ihl = 5;
   ip_header->tos =  0;
   ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
   ip_header->id = htons(12345);
   ip_header->ttl = 64;
   ip_header->protocol = IPPROTO_TCP;
   ip_header->frag_off = 0;
   ip_header->check = 0;

   return NULL;
}

void stealth_scanner(char *addr, const char *host, int start_port, int end_port,
             float timeout, int success_only) {
    atomic_init(&current_port, start_port);

}
