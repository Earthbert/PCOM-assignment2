#include "common.h"
#include "die.h"

#define MAX_CONNECTIONS 500

int receive_topic(int fd, struct udp_client_info *udp_info) {

	socklen_t udp_client_addr_len = sizeof(udp_info->addr);
	int rc = recvfrom(fd, &(udp_info->packet), sizeof(udp_info->packet), 0,
		(struct sockaddr *)&(udp_info->addr), &udp_client_addr_len);
		
	udp_info->payload_len = rc - sizeof(udp_client_info::packet.topic) -
		sizeof(udp_client_info::packet.data_type);

	return rc;
}

void handle_new_entry(udp_client_info *udp_info,
	std::unordered_map <std::string, std::vector<std::shared_ptr<tcp_client>>> &topics,
	std::unordered_map <std::string, std::shared_ptr<tcp_client>> &clients) {

	int topic_len = strlen(udp_info->packet.topic);
	if (topic_len > 50)
		topic_len = 50;
	int send_len = sizeof(app_header) + sizeof(sockaddr_in) +
		topic_len + udp_info->payload_len;
	std::shared_ptr<char> send_buff = std::shared_ptr<char>(new char[send_len]());

	// Prepare send buffer
	app_header *app_hdr = (app_header *)send_buff.get();
	app_hdr->sync = SEND_MSG;
	app_hdr->data_type = udp_info->packet.data_type;
	app_hdr->topic_len = topic_len;
	app_hdr->msg_len = udp_info->payload_len;
	memcpy(send_buff.get() + sizeof(app_header), &(udp_info->addr), sizeof(sockaddr_in));
	memcpy(send_buff.get() + sizeof(app_header) + sizeof(sockaddr_in),
		&(udp_info->packet.topic), topic_len);
	memcpy(send_buff.get() + sizeof(app_header) + sizeof(sockaddr_in) + topic_len,
		&(udp_info->packet.payload), udp_info->payload_len);

	// Send message and store it for later if subscriber is not connected
	auto topic_clients = topics[udp_info->packet.topic];
	for (auto topic_cli : topic_clients) {
		if (topic_cli.get()->fd != -1) {
			memcpy(&(app_hdr->client_id), topic_cli.get()->name.c_str(),
				strlen(topic_cli.get()->name.c_str()));
			send_all(topic_cli.get()->fd, send_buff.get(), send_len);
		} else if (topic_cli.get()->subscriptions[udp_info->packet.topic] == 1) {
			topic_cli.get()->msg_queue
				.push_back(std::pair(send_buff, send_len));
		}
	}
}

void handle_tcp_client_request(int cli_fd, app_header *app_hdr,
	std::unordered_map<std::string, std::vector<std::shared_ptr<tcp_client>>> &topics,
	std::unordered_map<std::string, std::shared_ptr<tcp_client>> &clients,
	std::unordered_map<int, sockaddr_in> &cli_ip_ports) {

	if (!clients.count(app_hdr->client_id)) {
		clients[app_hdr->client_id] = std::shared_ptr<tcp_client>(new tcp_client);
	}
	auto client = clients[app_hdr->client_id].get();

	// Check if client is already connected
	if (client->fd != -1 && client->fd != cli_fd) {
		app_hdr->sync = REFUSE_CONN;
		send_all(cli_fd, app_hdr, sizeof(*app_hdr));
		printf("Client %s already connected.\n", app_hdr->client_id);
		return;
	}
	// Check if client is has just started the connection
	if (app_hdr->sync == START_CONN) {
		client->name = app_hdr->client_id;
		client->fd = cli_fd;
		sockaddr_in cli_addr = cli_ip_ports[cli_fd];
		printf("New client %s connected from %hu:%s.\n", app_hdr->client_id,
			cli_addr.sin_port, inet_ntoa(cli_addr.sin_addr));
		while (!client->msg_queue.empty()) {
			auto msg = client->msg_queue.back();
			app_header *app_hdr = (app_header *)msg.first.get();
			strcpy(app_hdr->client_id, client->name.c_str());
			send_all(cli_fd, msg.first.get(), msg.second);
			client->msg_queue.pop_back();
		}
		return;
	}
	char topic[50] = { 0 };
	recv_all(cli_fd, topic, app_hdr->topic_len);
	// Handle subscribe request
	if (app_hdr->sync == SUBSCRIBE || app_hdr->sync == SUBSCRIBE_SF) {
		if (app_hdr->sync == SUBSCRIBE)
			client->subscriptions[topic] = 0;
		else
			client->subscriptions[topic] = 1;
		auto &topic_clients = topics[topic];
		bool client_found = false;
		for (auto topic_cli : topic_clients) {
			if (topic_cli.get() == client) {
				client_found = true;
				break;
			}
		}
		if (!client_found)
			topic_clients.push_back(clients[app_hdr->client_id]);
		return;
	}
	if (app_hdr->sync == UNSUBSCRIBE) {
		client->subscriptions.erase(topic);
		auto &topic_clients = topics[topic];
		for (auto &topic_cli : topic_clients) {
			if (topic_cli.get() == client) {
				std::swap(topic_cli, topic_clients.back());
				topic_clients.pop_back();
				break;
			}
		}
	}
}

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		printf("\n Usage: %s <PORT>\n", argv[0]);
		return 1;
	}

	uint16_t port;
	struct sockaddr_in servaddr;
	int tcp_fd, udp_fd, rc;

	struct udp_client_info udp_info;
	struct app_header app_hdr;

	std::unordered_map<std::string, std::vector<std::shared_ptr<tcp_client>>> topics;
	std::unordered_map<std::string, std::shared_ptr<tcp_client>> clients;
	std::unordered_map<int, sockaddr_in> cli_ip_ports;

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
		setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
		DIE(tcp_fd < 0, "setsockopt failed");

		setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
		DIE(udp_fd < 0, "setsockopt failed");
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
	std::vector<pollfd> poll_fds;

	// Add both server sockets and stdin to poll
	poll_fds.push_back(pollfd{ tcp_fd, POLLIN, 0 });
	poll_fds.push_back(pollfd{ udp_fd, POLLIN, 0 });
	poll_fds.push_back(pollfd{ STDIN_FILENO, POLLIN, 0 });

	while (1) {
		poll(poll_fds.data(), poll_fds.size(), -1);

		for (size_t i = 0; i < poll_fds.size(); i++) {
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

					int enable = 1;
					setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR | TCP_NODELAY, &enable, sizeof(int));
					DIE(newsockfd < 0, "setsockopt failed");

					cli_ip_ports[newsockfd] = cli_addr;
					poll_fds.push_back(pollfd{ newsockfd, POLLIN, 0 });
				} else if (poll_fds[i].fd == STDIN_FILENO) {
					// Read from stdin
					char line[100];
					fgets(line, 100, stdin);
					if (!strncmp(line, "exit", 4))
						goto exit;
				} else {
					// Received packet from tcp client
					int rc = recv_all(poll_fds[i].fd, &app_hdr, sizeof(app_hdr));
					DIE(rc < 0, "recv");

					if (rc == 0) {
						// Connection closed
						close(poll_fds[i].fd);
						for (auto it = clients.begin(); it != clients.end(); it++) {
							if (it->second.get()->fd == poll_fds[i].fd) {
								it->second.get()->fd = -1;
								printf("Client %s disconnected.\n", it->first.c_str());
							}
						}
						cli_ip_ports.erase(poll_fds[i].fd);
						poll_fds.erase(poll_fds.begin() + i);
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
	for (size_t i = 3; i < poll_fds.size(); i++)
		close(poll_fds[i].fd);
	return 0;
}
