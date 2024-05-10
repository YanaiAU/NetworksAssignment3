#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
// RUDP packet structure
struct rudp_packet {
    int flags;         // Flags for SYN, ACK, etc.
    int length;       // Length of the packet
    int checksum;     // Checksum for error checking
    int sequence_num; // Sequence number for data packets
    int data[1024]; // Data payload
};

// Function prototypes
int rudp_socket();
int rudp_bind(int sockfd, int port);
int rudp_send(int sockfd, const char *data, int len);
int rudp_recv(int sockfd, char *buffer, int buffer_len);
int rudp_close(int sockfd);
