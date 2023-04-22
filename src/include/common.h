#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

#include <string.h>
#include <map>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define PAYLOAD_MAXSIZE 1500

enum subject: int8_t {
	SEND_MSG = 0,
	START_CONN = 1,
	SUBSCRIBE = 2,
	SUBSCRIBE_SF = 3,
	UNSUBSCRIBE = 4,
	REFUSE_CONN = -1
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
	uint16_t msg_len;
};

typedef struct {
	bool operator()(const char *s1, const char *s2) const {
		return strcmp(s1, s2);
	}
} cmp_char_array;

typedef struct {
	int fd;
	std::map<char *, uint8_t, cmp_char_array> subscriptions;
} tcp_client;

#endif
