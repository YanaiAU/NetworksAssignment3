#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "RUDP_API.h"

int rudp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failure");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

int rudp_bind(int sockfd, int port) {
    struct sockaddr_in servaddr;

    // Initialize server address structure
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("Bind failed");
        close(sockfd);
        return -1;
    }

    return 0;
}

int rudp_send(int sockfd, const char *data, int len) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Example: sending to localhost
    server_addr.sin_port = htons(12345); // Example: using port 12345

    int bytes_sent = sendto(sockfd, data, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_sent < 0) {
        perror("rudp_send");
        return -1;
    }
    return bytes_sent;
}

int rudp_recv(int sockfd, char *buffer, int buffer_len) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int bytes_received = recvfrom(sockfd, buffer, buffer_len - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
    if (bytes_received < 0) {
        perror("rudp_recv");
        return -1;
    }
    buffer[bytes_received] = '\0'; // Null-terminate the received data
    return bytes_received;
}

int rudp_close(int sockfd) {
    int close_status = close(sockfd);
    if (close_status < 0) {
        perror("rudp_close");
        return -1;
    }
    return 0;
}