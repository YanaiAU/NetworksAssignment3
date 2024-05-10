#include <stdio.h>
#include <stdlib.h>
#include "RUDP_API.h"

#define BUFFER_SIZE 1024

int main() {
    // Create a RUDP socket
    int sockfd = rudp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }

    // Bind the socket
    if (rudp_bind(sockfd, 12345) < 0) {
        fprintf(stderr, "Error binding socket\n");
        exit(EXIT_FAILURE);
    }

    // Receive buffer
    char buffer[BUFFER_SIZE];

    // Receive data
    int bytes_received = rudp_recv(sockfd, buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        fprintf(stderr, "Error receiving data\n");
        exit(EXIT_FAILURE);
    } else {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("Received %d bytes: %s\n", bytes_received, buffer);
    }

    // Close the socket
    if (rudp_close(sockfd) < 0) {
        fprintf(stderr, "Error closing socket\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
