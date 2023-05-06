#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>

int recv_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = (char *)buffer;

    while (bytes_remaining) {
        ssize_t rec;
        rec = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (rec <= 0)
            return rec;
        bytes_received += rec;
        bytes_remaining -= rec;
    }
    return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = (char *)buffer;

    while (bytes_remaining) {
        ssize_t rec;
        rec = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (rec == -1)
            return -1;
        bytes_sent += rec;
        bytes_remaining -= rec;
    }

    return bytes_sent;
}