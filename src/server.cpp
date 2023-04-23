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

#define MAX_FDS 100

static FILE *server_log;

int receive_topic(int fd, struct udp_client_info *udp_info) {
	socklen_t udp_client_addr_len = sizeof(udp_info->addr);
	int rc = recvfrom(fd, &(udp_info->packet), sizeof(udp_info->packet), 0,
		(struct sockaddr *)&(udp_info->addr), &udp_client_addr_len);
	udp_info->payload_len = rc - sizeof(udp_client_info::packet.topic) -
		sizeof(udp_client_info::packet.data_type);
	return rc;
}

void handle_new_entry(udp_client_info *udp_info,
	std::map < std::string, std::vector<std::shared_ptr<tcp_client>> > &topics,
	std::map < std::string, tcp_client> &clients) {
	int topic_len = strlen(udp_info->packet.topic);
	int send_len = sizeof(app_header) + sizeof(sockaddr_in) +
		topic_len + udp_info->payload_len;
	char *send_buff = new char[send_len];

	// Prepare send buffer
	app_header *app_hdr = (app_header *)send_buff;
	app_hdr->sync = SEND_MSG;
	app_hdr->topic_len = topic_len;
	app_hdr->msg_len = udp_info->payload_len;
	memcpy(send_buff + sizeof(app_header), &(udp_info->addr), sizeof(sockaddr_in));
	memcpy(send_buff + sizeof(app_header) + sizeof(sockaddr_in),
		&(udp_info->packet.topic), topic_len);
	memcpy(send_buff + sizeof(app_header) + sizeof(sockaddr_in) + topic_len,
		&(udp_info->packet.payload), udp_info->payload_len);

	auto topic_clients = topics[udp_info->packet.topic];
	for (auto &topic_cli : topic_clients) {
		if (topic_cli.get()->fd != -1) {
			memcpy(&(app_hdr->client_id), topic_cli.get()->name.c_str(),
				strlen(topic_cli.get()->name.c_str()));
			send_all(topic_cli.get()->fd, send_buff, send_len);
		} else if (topic_cli.get()->subscriptions[udp_info->packet.topic] == 1) {
			topic_cli.get()->msg_queue
				.push_back(std::pair(std::shared_ptr<char>(send_buff), send_len));
		}
	}
}

void handle_tcp_client_request(int cli_fd, app_packet *app_packet,
	std::map < std::string, std::vector<std::shared_ptr<tcp_client>> > &topics,
	std::map < std::string, tcp_client> &clients,
	std::map < int, sockaddr_in> &cli_ip_ports) {
	tcp_client client = clients[app_packet->hdr.client_id];

	// Check if client is already connected
	if (client.fd != -1 && client.fd != cli_fd) {
		app_packet->hdr.sync = REFUSE_CONN;
		send_all(cli_fd, app_packet, sizeof(*app_packet));
		return;
	}
	// Check if client is has just started the connection
	if (app_packet->hdr.sync == START_CONN) {
		client.name = app_packet->hdr.client_id;
		client.fd = cli_fd;
		sockaddr_in cli_addr = cli_ip_ports[cli_fd];
		printf("New client %s connected from %hd:%s\n.", app_packet->hdr.client_id,
			cli_addr.sin_port, inet_ntoa(cli_addr.sin_addr));
		while (!client.msg_queue.empty()) {
			auto msg = client.msg_queue.back();
			app_header *app_hdr = (app_header *)msg.first.get();
			strcpy(app_hdr->client_id, client.name.c_str());
			send_all(cli_fd, msg.first.get(), msg.second);
			client.msg_queue.pop_back();
		}
		return;
	}
	char topic[50] = { 0 };
	memcpy(topic, app_packet->payload, app_packet->hdr.topic_len);
	// Handle subscribe request
	if (app_packet->hdr.sync == SUBSCRIBE || app_packet->hdr.sync == SUBSCRIBE_SF) {
		if (app_packet->hdr.sync == SUBSCRIBE)
			client.subscriptions[topic] = 0;
		else
			client.subscriptions[topic] = 1;
		auto topic_clients = topics[topic];
		bool client_found = false;
		for (auto &topic_cli : topic_clients) {
			if (topic_cli.get() == &client) {
				client_found = true;
				break;
			}
		}
		if (!client_found)
			topic_clients.push_back(std::shared_ptr<tcp_client>(&client));
		return;
	}
	if (app_packet->hdr.sync == UNSUBSCRIBE) {
		client.subscriptions.erase(topic);
		auto topic_clients = topics[topic];
		for (auto &topic_cli : topic_clients) {
			if (topic_cli.get() == &client) {
				std::swap(topic_cli, topic_clients.back());
				topic_clients.pop_back();
				break;
			}
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
	struct app_packet app_hdr;

	std::map < std::string, std::vector<std::shared_ptr<tcp_client>> > topics;
	std::map < std::string, tcp_client> clients;
	std::map < int, sockaddr_in> cli_ip_ports;

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
	rc = listen(tcp_fd, MAX_FDS);
	DIE(rc < 0, "listen");

	// Set up poll
	struct pollfd poll_fds[MAX_FDS];
	int nr_fds = 3;
	// Add both server sockets to poll
	poll_fds[0].fd = tcp_fd;
	poll_fds[0].events = POLLIN;
	poll_fds[1].fd = udp_fd;
	poll_fds[1].events = POLLIN;
	poll_fds[2].fd = STDIN_FILENO;
	poll_fds[2].events = POLLIN;

	while (1) {
		poll(poll_fds, nr_fds, -1);

		for (int i = 0; i < nr_fds; i++) {
			if (poll_fds[i].revents & POLLIN) {
				if (poll_fds[i].fd == udp_fd) {
					rc = receive_topic(poll_fds[i].fd, &udp_info);
					if (rc > 0)
						handle_new_entry(&udp_info, topics, clients);
					else {
						fprintf(stderr, "udp client topic submission error\n");
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

					cli_ip_ports[poll_fds[i].fd] = cli_addr;

					fprintf(server_log, "New from %s : %d, at %d socket\n",
						inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				} else if (poll_fds[i].fd == STDIN_FILENO) {
					// Read from stdin
					char command[20];
					scanf("%s", command);
					if (!strcmp(command, "exit"))
						goto exit;
				} else {
					// Received packet from tcp client
					int rc = recv_all(poll_fds[i].fd, &app_hdr, sizeof(app_hdr));
					DIE(rc < 0, "recv");

					if (rc == 0) {
						// Connection closed
						close(poll_fds[i].fd);
						for (auto it = clients.begin(); it != clients.end(); i++) {
							if (it->second.fd == poll_fds[i].fd) {
								it->second.fd = -1;
								printf("Client %s disconected.", it->first.c_str());
							}
						}
						cli_ip_ports.erase(poll_fds[i].fd);
						memmove(&poll_fds[i], &poll_fds[i + 1], sizeof(struct pollfd) * (nr_fds - i - 1));
						nr_fds--;
					} else {
						handle_tcp_client_request(poll_fds[i].fd, &app_hdr, topics, clients, cli_ip_ports);
					}
				}
			}
		}
	}

exit:
	close(poll_fds[0].fd);
	close(poll_fds[1].fd);
	for (int i = 3; i < nr_fds; i++)
		close(poll_fds[i].fd);
	return 0;
}
