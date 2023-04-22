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

#include "die.h"
#include "common.h"

#define MAX_CONNECTIONS 100

int receive_topic(int fd, struct udp_client_info *udp_info) {
	socklen_t udp_client_addr_len = sizeof(udp_info->addr);
	int rc = recvfrom(fd, &(udp_info->packet), sizeof(udp_info->packet), 0,
		(struct sockaddr *)&(udp_info->addr), &udp_client_addr_len);

	return rc;
}

void handle_topic(udp_client_info *udp_info) {
	printf("%s %hhd %s\n", udp_info->packet.topic, udp_info->packet.data_type, udp_info->packet.payload);
}

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		printf("\n Usage: %s <port>\n", argv[0]);
		return 1;
	}

	// Disable stdout buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	struct sockaddr_in servaddr;
	int tcp_fd, udp_fd, rc;

	struct udp_client_info udp_p;

	// Save server port number as number
	uint16_t port;
	rc = sscanf(argv[1], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

	// Get TCP socket
	tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_fd < 0, "tcp socket");

	// Get UDP socket
	udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_fd < 0, "udp socket");

	// Make port reusable, in case we run this really fast two times in a row
	{
		int enable = 1;
		setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR | TCP_NODELAY, &enable, sizeof(int));
		DIE(tcp_fd < 0, "setsockopt(SO_REUSEADDR) failed");

		setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
		DIE(udp_fd < 0, "setsockopt(SO_REUSEADDR) failed");
	}

	// Fill the details on what destination port should be
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

	// Bind the both sockets with the server address.
	rc = bind(tcp_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
	DIE(rc < 0, "tcp bind failed");
	rc = bind(udp_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
	DIE(rc < 0, "udp failed");

	// Set up poll
	struct pollfd poll_fds[MAX_CONNECTIONS];
	int nr_conn = 2;
	// Add both server sockets to poll
	poll_fds[0].fd = tcp_fd;
	poll_fds[0].events = POLLIN;
	poll_fds[1].fd = udp_fd;
	poll_fds[1].events = POLLIN;

	while (1) {
		poll(poll_fds, nr_conn, -1);

		for (int i = 0; i < nr_conn; i++) {
			if (poll_fds[i].revents & POLLIN) {
				if (poll_fds[i].fd == udp_fd) {
					rc = receive_topic(poll_fds[i].fd, &udp_p);
					if (rc > 0)
						handle_topic(&udp_p);
					else {
						fprintf(stderr, "udp client topic submission error\n");
					}
				}
			}
		}
	}

	return 0;
}
