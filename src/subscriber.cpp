#include "common.h"
#include "die.h"

#define MAX_FDS 2

void send_start_conn_request(int sock_fd, char *client_id) {
	int send_len = sizeof(app_header);
	char *send_buff = new char[send_len]();

	app_header *app_hdr = (app_header *)send_buff;
	memcpy(&(app_hdr->client_id), client_id, 10);
	app_hdr->sync = START_CONN;
	app_hdr->topic_len = 0;
	app_hdr->msg_len = 0;

	send_all(sock_fd, send_buff, send_len);
}

void send_subscribe_request(int sock_fd, char *client_id, char *topic, int sf_option) {
	int topic_len = strlen(topic);
	int send_len = sizeof(app_header) + topic_len;
	char *send_buff = new char[send_len]();

	app_header *app_hdr = (app_header *)send_buff;
	memcpy(&(app_hdr->client_id), client_id, 10);
	if (sf_option)
		app_hdr->sync = SUBSCRIBE_SF;
	else
		app_hdr->sync = SUBSCRIBE;
	app_hdr->topic_len = topic_len;

	memcpy(send_buff + sizeof(app_header), topic, topic_len);
	send_all(sock_fd, send_buff, send_len);
	printf("Subscribed to topic.\n");
}

void send_unsubscribe_request(int sock_fd, char *client_id, char *topic) {
	int topic_len = strlen(topic);
	int send_len = sizeof(app_header) + topic_len;
	char *send_buff = new char[send_len]();

	app_header *app_hdr = (app_header *)send_buff;
	memcpy(&(app_hdr->client_id), client_id, 10);
	app_hdr->sync = UNSUBSCRIBE;
	app_hdr->topic_len = topic_len;

	memcpy(send_buff + sizeof(app_header), topic, topic_len);
	send_all(sock_fd, send_buff, send_len);
	printf("Unsubscribed to topic.\n");
}

int receive_app_packet(int sock_fd, char *client_id) {
	app_header app_hdr;
	int rc;
	rc = recv_all(sock_fd, &app_hdr, sizeof(app_hdr));
	DIE(rc < 0, "recv");

	// Connection closed or refused
	if (rc == 0 || app_hdr.sync == REFUSE_CONN)
		return -1;

	sockaddr_in udp_client_addr;
	rc = recv_all(sock_fd, &udp_client_addr, sizeof(udp_client_addr));
	DIE(rc < 0, "recv");
	char topic[51] = { 0 };
	rc = recv_all(sock_fd, topic, app_hdr.topic_len);
	DIE(rc < 0, "recv");
	char *message = new char[app_hdr.msg_len];
	rc = recv_all(sock_fd, message, app_hdr.msg_len);
	DIE(rc < 0, "recv");

	char addr_string[16];
	inet_ntop(AF_INET, &(udp_client_addr.sin_addr), addr_string, INET_ADDRSTRLEN);

	switch (app_hdr.data_type) {
	case 0: {
		uint8_t sign = *((uint8_t *)(message));
		int64_t number = ntohl(*((uint32_t *)(message + 1)));
		if (sign == 1)
			number = -number;
		printf("%s - INT - %ld\n", topic, number);
	}break;
	case 1: {
		float number = ntohs(*((uint16_t *)(message)));
		number /= 100;
		printf("%s - SHORT_REAL - %.2f\n", topic, number);
	}break;
	case 2: {
		uint8_t sign = *((uint8_t *)(message));
		float number = ntohl(*((uint32_t *)(message + 1)));
		uint8_t pow = *((uint8_t *)(message + 5));
		while (pow) {
			number /= 10;
			pow--;
		}
		if (sign == 1)
			number = -number;
		printf("%s - FLOAT - %f\n", topic, number);

	}break;
	case 3: {
		printf("%s - STRING - %s\n", topic, message);
	}break;
	}
	return 1;
}

int main(int argc, char const *argv[]) {
	if (argc != 4) {
		printf("\n Usage: %s <CLIENT_ID> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
		return 1;
	}

	char client_id[20];
	uint16_t port;
	struct sockaddr_in servaddr;
	int rc, sock_fd;

	// Save client id
	strcpy(client_id, argv[1]);

	// Disable stdout buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// Save server port number as number
	rc = sscanf(argv[3], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

	// Get TCP socket
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sock_fd < 0, "tcp socket");

	// Make port reusable, in case we run this really fast two times in a row
	{
		int enable = 1;
		setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | TCP_NODELAY, &enable, sizeof(int));
		DIE(sock_fd < 0, "setsockopt(SO_REUSEADDR) failed");
	}

	// Fill the details on what destination port should be
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(argv[2]);;
	servaddr.sin_port = htons(port);

	// Set up poll
	struct pollfd poll_fds[MAX_FDS];
	int nr_fds = 2;
	// Add socket and stdin to poll
	poll_fds[0].fd = sock_fd;
	poll_fds[0].events = POLLIN;
	poll_fds[1].fd = STDIN_FILENO;
	poll_fds[1].events = POLLIN;

	// Connect to server
	rc = connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	DIE(rc < 0, "connect");

	send_start_conn_request(sock_fd, client_id);

	while (1) {
		poll(poll_fds, nr_fds, -1);

		for (int i = 0; i < nr_fds; i++) {
			if (poll_fds[i].revents & POLLIN) {
				if (poll_fds[i].fd == STDIN_FILENO) {
					char line[100];
					fgets(line, 100, stdin);
					char *command = strtok(line, " ");
					rc = sscanf(line, "%s", command);
					if (!strcmp(command, "subscribe")) {
						char *topic = strtok(NULL, " ");
						int sf_option = atoi(strtok(NULL, " "));
						send_subscribe_request(sock_fd, client_id, topic, sf_option);
					} else if (!strcmp(command, "unsubscribe")) {
						char topic[50];
						sscanf(line, "%s", topic);
						send_unsubscribe_request(sock_fd, client_id, topic);
					} else if (!strcmp(command, "exit"))
						goto exit;
				} else {
					rc = receive_app_packet(sock_fd, client_id);
					if (rc == -1)
						goto exit;
				}
			}
		}
	}
exit:
	close(sock_fd);
	return 0;
}
