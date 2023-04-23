#ifndef __COMMON_H__
#define __COMMON_H__

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include <string>
#include <map>
#include <vector>
#include <memory>

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
	int payload_len;
};

struct __attribute__((__packed__)) app_header {
	char client_id[10];
	subject sync;
	uint8_t data_type;
	uint8_t topic_len;
	uint16_t msg_len;
};

struct tcp_client {
	tcp_client() {
		fd = -1;
	}
	std::string name;
	int fd;
	std::map<std::string, uint8_t> subscriptions;
	std::vector<std::pair<std::shared_ptr<char>, int>> msg_queue;
};

#endif
