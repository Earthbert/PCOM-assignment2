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

#include "die.h"
#include "common.h"

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		printf("\n Usage: %s <port>\n", argv[0]);
		return 1;
	}

	struct sockaddr_in servaddr;
	int tcp_fd, udp_fd, rc;

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
		setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
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

	

	return 0;
}
