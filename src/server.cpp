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

#include "die.h"
#include "common.h"

#define MAX_CONNECTIONS 100

int receive_topic(int fd, struct udp_client_info *udp_info) {
	socklen_t udp_client_addr_len = sizeof(udp_info->addr);
	int rc = recvfrom(fd, &(udp_info->packet), sizeof(udp_info->packet), 0,
		(struct sockaddr *)&(udp_info->addr), &udp_client_addr_len);

	return rc;
}

void handle_topic(udp_client_info *udp_info,
	std::map < char *, std::vector<std::shared_ptr<tcp_client>>, cmp_char_array > topics,
	std::map < char *, tcp_client, cmp_char_array> clients) {

}

void handle_tcp_client_request(int client_fd, app_header *app_hdr,
	std::map < char *, std::vector<std::shared_ptr<tcp_client>>, cmp_char_array > topics,
	std::map < char *, tcp_client, cmp_char_array> clients) {
	tcp_client client = clients[app_hdr->client_id];

	// Check if client is already connected
	if (client.fd != client_fd) {
		app_hdr->sync = REFUSE_CONN;
		send_all(client_fd, app_hdr, sizeof(*app_hdr));
		return;
	}
	// Check if client is has just started the connection
	if (app_hdr->sync == START_CONN) {
		client.fd = client_fd;
		return;
	}
	if (app_hdr->sync == SUBSCRIBE) {
		if (!client.subscriptions.count(app_hdr->client_id)) {
			
		}
	}
}

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		printf("\n Usage: %s <port>\n", argv[0]);
		return 1;
	}

	uint16_t port;
	struct sockaddr_in servaddr;
	int tcp_fd, udp_fd, rc;

	struct udp_client_info udp_info;
	struct app_header app_hdr;

	std::map < char *, std::vector<std::shared_ptr<tcp_client>>, cmp_char_array > topics;
	std::map < char *, tcp_client, cmp_char_array> clients;

	// Create server log
	FILE *server_log = fopen("server.log", "w");

	// Disable stdout buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// Save server port number as number
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

	// Make tcp_fd passive
	rc = listen(tcp_fd, MAX_CONNECTIONS);
	DIE(rc < 0, "listen");

	// Set up poll
	struct pollfd poll_fds[MAX_CONNECTIONS];
	int nr_fds = 2;
	// Add both server sockets to poll
	poll_fds[0].fd = tcp_fd;
	poll_fds[0].events = POLLIN;
	poll_fds[1].fd = udp_fd;
	poll_fds[1].events = POLLIN;

	while (1) {
		poll(poll_fds, nr_fds, -1);

		for (int i = 0; i < nr_fds; i++) {
			if (poll_fds[i].revents & POLLIN) {
				if (poll_fds[i].fd == udp_fd) {
					rc = receive_topic(poll_fds[i].fd, &udp_info);
					if (rc > 0)
						handle_topic(&udp_info, topics, clients);
					else {
						fprintf(server_log, "udp client topic submission error\n");
					}
				} else if (poll_fds[i].fd == tcp_fd) {
					// Received new connection
					struct sockaddr_in cli_addr;
					socklen_t cli_len = sizeof(cli_addr);
					int newsockfd = accept(tcp_fd, (struct sockaddr *)&cli_addr, &cli_len);
					DIE(newsockfd < 0, "accept");

					poll_fds[nr_fds].fd = newsockfd;
					poll_fds[nr_fds].events = POLLIN;
					nr_fds++;

					fprintf(server_log, "New from %s : %d, at %d socket\n",
						inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				} else {
					// Received packet from tcp client
					int rc = recv_all(poll_fds[i].fd, &app_hdr, sizeof(app_hdr));
					DIE(rc < 0, "recv");

					if (rc == 0) {
						// Connection closed
						close(poll_fds[i].fd);

						memmove(&poll_fds[i], &poll_fds[i + 1], sizeof(struct pollfd) * (nr_fds - i - 1));
						nr_fds--;
						fprintf(server_log, "Connecton closed with client at %d socket\n", poll_fds[i].fd);
					} else {
						handle_tcp_client_request(poll_fds[i].fd, &app_hdr, topics, clients);
					}
				}
			}
		}
	}

	return 0;
}
