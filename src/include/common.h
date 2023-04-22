#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define PAYLOAD_MAXSIZE 1500

struct __attribute__((__packed__)) udp_client_packet {
  char topic[50];
  u_int8_t data_type;
  char payload[PAYLOAD_MAXSIZE];
};

struct udp_client_info {
  struct udp_client_packet packet;
  sockaddr_in addr;
};

#endif
