#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
/*
    TODO 1.1: Rescrieți funcția de mai jos astfel încât ea să facă primirea
    a exact len octeți din buffer.
*/
int recv_all(int sockfd, void *buffer, size_t len) {

    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = (char *)buffer;

    while (bytes_remaining) {
        ssize_t rec;
        rec = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (rec == -1)
            return -1;
        bytes_received += rec;
        bytes_remaining -= rec;
    }
    return bytes_received;
}

/*
    TODO 1.2: Rescrieți funcția de mai jos astfel încât ea să facă trimiterea
    a exact len octeți din buffer.
*/

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