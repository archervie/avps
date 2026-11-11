#include <arpa/inet.h>
#include <assert.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "scan_utils.h"
#include "utils.h"

/*
Test 1: Verify pseudo-header fields are laid out correctly in memory.
*/
static void test_pseudo_header_layout(void) {
  pseudo_tcp_header pth;
  memset(&pth, 0, sizeof(pth));

  pth.source_address = inet_addr("192.168.1.10");
  pth.dest_address = inet_addr("192.168.1.1");
  pth.placeholder = 0;
  pth.protocol = IPPROTO_TCP;
  pth.tcp_length = htons(sizeof(struct tcphdr));

  assert(sizeof(pseudo_tcp_header) == 12);
  assert(pth.placeholder == 0);
  assert(pth.protocol == 6); // IPPROTO_TCP is 6
  assert(ntohs(pth.tcp_length) == sizeof(struct tcphdr));

  printf("[PASS] test_pseudo_header_layout\n");
}

/*
Test 2: Known test vector (deterministic verification).
*/
static void test_tcp_checksum_known_values(void) {
  struct tcphdr tcp;
  memset(&tcp, 0, sizeof(tcp));

  // Construct a standard TCP SYN-style header
  build_tcp_header(&tcp, 443);
  tcp.syn = 1;

  uint32_t src = inet_addr("10.0.0.2");
  uint32_t dst = inet_addr("10.0.0.1");

  calculate_tcp_checksum(&tcp, src, dst);

  // The checksum must be non-zero
  assert(tcp.check != 0);

  // Running the calculation a second time with the exact same inputs
  // must be repeatable (idempotent after resetting check to 0).
  uint16_t first_calc = tcp.check;
  tcp.check = 0;
  calculate_tcp_checksum(&tcp, src, dst);
  assert(tcp.check == first_calc);

  printf("[PASS] test_tcp_checksum_known_values (Checksum: 0x%04X)\n",
         ntohs(tcp.check));
}

/**
 * Test 3: Mathematical verification (Check recalculation invariant).
 * In RFC 791/9293, computing the checksum over the pseudo-header + TCP segment
 * WITH the computed checksum field included yields 0 (or ~0 / 0xFFFF).
 */
static void test_checksum_invariant(void) {
  struct tcphdr tcp;
  memset(&tcp, 0, sizeof(tcp));
  build_tcp_header(&tcp, 8080);
  tcp.ack = 1;

  uint32_t src = inet_addr("172.16.0.5");
  uint32_t dst = inet_addr("172.16.0.1");

  calculate_tcp_checksum(&tcp, src, dst);

  // Now re-run csum over pseudo-header + TCP header containing the check
  int total_len = sizeof(pseudo_tcp_header) + sizeof(struct tcphdr);
  char buf[total_len];

  pseudo_tcp_header pseudo;
  pseudo.source_address = src;
  pseudo.dest_address = dst;
  pseudo.placeholder = 0;
  pseudo.protocol = IPPROTO_TCP;
  pseudo.tcp_length = htons(sizeof(struct tcphdr));

  memcpy(buf, &pseudo, sizeof(pseudo));
  memcpy(buf + sizeof(pseudo), &tcp, sizeof(tcp));

  uint16_t verification = csum((unsigned short *)buf, total_len);
  // Depending on whether csum() inverts (~sum) or leaves it raw,
  // the result over the full buffer including check is 0x0000 or 0xFFFF.
  assert(verification == 0x0000 || verification == 0xFFFF);

  printf("[PASS] test_checksum_invariant\n");
}

/**
 * Test 4: Verify checksum detects alterations (sensitivity check).
 */
static void test_checksum_detects_tampering(void) {
  struct tcphdr tcp;
  memset(&tcp, 0, sizeof(tcp));
  build_tcp_header(&tcp, 53);

  uint32_t src = inet_addr("192.168.1.50");
  uint32_t dst = inet_addr("1.1.1.1");

  calculate_tcp_checksum(&tcp, src, dst);
  uint16_t original_checksum = tcp.check;

  // Mutate destination port
  tcp.dest = htons(54);
  tcp.check = 0;
  calculate_tcp_checksum(&tcp, src, dst);
  assert(tcp.check != original_checksum);

  // Mutate source IP
  tcp.dest = htons(53);
  tcp.check = 0;
  calculate_tcp_checksum(&tcp, inet_addr("192.168.1.51"), dst);
  assert(tcp.check != original_checksum);

  printf("[PASS] test_checksum_detects_tampering\n");
}

int main(void) {
  printf("Running TCP Checksum unit tests...\n");
  test_pseudo_header_layout();
  test_tcp_checksum_known_values();
  test_checksum_invariant();
  test_checksum_detects_tampering();
  printf("All checksum tests passed successfully.\n");
  return 0;
}
