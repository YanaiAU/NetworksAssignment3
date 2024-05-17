#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "RUDP_API.h"

int rudp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failure");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

int rudp_bind(int sockfd, int port, struct sockaddr_in addr) {
    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Bind failed");
        close(sockfd);
        return -1;
    }

    return 0;
}

int rudp_send(int sockfd, struct RUDP_Packet *packet, struct sockaddr_in addr) {
    // Calculate checksum for the packet
    packet->checksum = calculate_checksum(packet);
    if (sendto(sockfd, packet, sizeof(struct RUDP_Packet), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to send packet\n");
        return -1;
    }
    return 1;
}

int rudp_recv(int sockfd, struct RUDP_Packet *packet, struct sockaddr_in *addr) {
    socklen_t addr_len = sizeof(*addr);
    int bytes_received = recvfrom(sockfd, packet, sizeof(struct RUDP_Packet), 0, (struct sockaddr *)addr, &addr_len);
    if (bytes_received < 0) {
        perror("rudp_recv");
        return -1;
    }
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

// Function to calculate checksum using XOR
uint16_t calculate_checksum(const struct RUDP_Packet *packet) {
    const uint8_t *data = (const uint8_t *)packet;
    size_t length = sizeof(struct RUDP_Packet);
    uint16_t checksum = 0;

    // Perform XOR operation on each byte of the packet
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }

    return checksum;
}

