#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

// RUDP packet structure
struct __attribute__((packed)) RUDP_Packet {
    uint16_t checksum; // 2 bytes for checksum
    uint8_t flags;     // 1 byte for flags
    uint16_t length;   // 2 bytes for length
    uint16_t seq_num;    // 2 byte for sequence number
    char data[1017];   // 1019 bytes of data
};

// Function prototypes
int rudp_socket();
int rudp_bind(int sockfd, int port, struct sockaddr_in addr);
int rudp_send(int sockfd, struct RUDP_Packet *packet, struct sockaddr_in addr);
int rudp_recv(int sockfd, struct RUDP_Packet *packet, struct sockaddr_in *addr);
int rudp_close(int sockfd);
uint16_t calculate_checksum(const struct RUDP_Packet *packet);