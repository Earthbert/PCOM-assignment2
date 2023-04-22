#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define PAYLOAD_MAXSIZE 1500

enum subject : int8_t {
	SEND_MSG = 0,
	SUBSCRIBE = 1,
	SUBSCRIBE_SF = 2,
	UNSUBSCRIBE = 3
};

struct __attribute__((__packed__)) udp_client_packet {
	char topic[50];
	u_int8_t data_type;
	char payload[PAYLOAD_MAXSIZE];
};

struct udp_client_info {
	struct udp_client_packet packet;
	sockaddr_in addr;
};

struct __attribute__((__packed__)) app_header {
	char client_id[10];
	subject sync;	
	uint8_t topic_len;
	uint8_t msg_len;
};

#endif
