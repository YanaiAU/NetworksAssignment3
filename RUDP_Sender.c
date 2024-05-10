#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RUDP_API.h"

int main() {
    // Create a RUDP socket
    int sockfd = rudp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }

    // Data to send
    const char *data = "Hello, receiver!";
    int data_length = strlen(data);

    // Send data
    int bytes_sent = rudp_send(sockfd, data, data_length);
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending data\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Sent %d bytes: %s\n", bytes_sent, data);
    }

    // Close the socket
    if (rudp_close(sockfd) < 0) {
        fprintf(stderr, "Error closing socket\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
