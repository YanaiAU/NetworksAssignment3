#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "RUDP_API.h"

#define SYN_FLAG 2
#define ACK_FLAG 1
#define FIN_FLAG 4
#define START_FILE_FLAG 8
#define FILE_SIZE_BYTES (2 * 1024 * 1024) // 2 MB in bytes

double get_time_in_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Starting Receiver...\n");

    int port = atoi(argv[1]);

    int sockfd = rudp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    receiver_addr.sin_port = htons(port);

    if (rudp_bind(sockfd, port, receiver_addr) < 0) {
        fprintf(stderr, "Error binding socket\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    int received_syn = 0;
    int seq_num = 0;
    char received_data[FILE_SIZE_BYTES];
    memset(received_data, 0, FILE_SIZE_BYTES);

    size_t total_received = 0;

    while (1) {
        struct RUDP_Packet packet;
        int bytes_received = rudp_recv(sockfd, &packet, &sender_addr);
        if (bytes_received < 0) {
            continue;
        }

        uint16_t received_checksum = packet.checksum;
        packet.checksum = 0;
        uint16_t calculated_checksum = calculate_checksum(&packet);

        if (received_checksum == calculated_checksum) {
            if (packet.flags & SYN_FLAG && !received_syn) {
                printf("Waiting for RUDP connection...\n");
                printf("Connection request received, sending ACK.\n");
                struct RUDP_Packet syn_ack_packet;
                memset(&syn_ack_packet, 0, sizeof(syn_ack_packet));
                syn_ack_packet.flags = SYN_FLAG | ACK_FLAG;
                syn_ack_packet.checksum = htons(0);
                syn_ack_packet.length = htons(0);
                syn_ack_packet.seq_num = 0;
                rudp_send(sockfd, &syn_ack_packet, sender_addr);
                received_syn = 1;
                printf("Sender connected.\n");
            } else if (packet.flags & START_FILE_FLAG) {
                printf("Beginning to receive file...\n");
                total_received = 0;
                seq_num = 0;
            } else if (packet.flags & FIN_FLAG) {
                struct RUDP_Packet fin_ack_packet;
                memset(&fin_ack_packet, 0, sizeof(fin_ack_packet));
                fin_ack_packet.flags = FIN_FLAG | ACK_FLAG;
                fin_ack_packet.checksum = htons(0);
                fin_ack_packet.length = htons(0);
                fin_ack_packet.seq_num = 0;
                rudp_send(sockfd, &fin_ack_packet, sender_addr);
                printf("Sender sent exit message.\n");
                printf("ACK sent.\n");
                break;
            } else if (packet.seq_num == seq_num) {
                printf("Received packet with seq_num %d\n", packet.seq_num);
                printf("%zu\n", total_received);
                memcpy(received_data + (seq_num * ntohs(packet.length)), packet.data, ntohs(packet.length));

                struct RUDP_Packet ack_packet;
                memset(&ack_packet, 0, sizeof(ack_packet));
                ack_packet.flags = ACK_FLAG;
                ack_packet.checksum = htons(0);
                ack_packet.length = htons(0);
                ack_packet.seq_num = packet.seq_num;
                rudp_send(sockfd, &ack_packet, sender_addr);
                printf("ACK sent for packet %d\n", packet.seq_num);
                total_received += ntohs(packet.length);
                seq_num++;
            } else {
                printf("Unexpected packet with seq_num %d\n", packet.seq_num);
            }
        } else {
            printf("Checksum mismatch, packet ignored\n");
        }
    }

    printf("Received data: %s\n", received_data);

    if (rudp_close(sockfd) < 0) {
        fprintf(stderr, "Error closing socket\n");
        exit(EXIT_FAILURE);
    }

    printf("Receiver end.\n");

    return 0;
}
