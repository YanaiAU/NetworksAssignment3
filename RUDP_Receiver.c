#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "RUDP_API.h"

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

    int received_syn = 0;
    size_t total_received = 0;
    while (1) {
        double start_time, end_time;
        struct RUDP_Packet packet;
        int bytes_received = rudp_recv(sockfd, &packet, &receiver_addr);
        if (bytes_received < 0) {
            continue;
        }

        uint16_t received_checksum = packet.checksum;
        packet.checksum = 0;
        uint16_t calculated_checksum = calculate_checksum(&packet);

        if (received_checksum == calculated_checksum) {
            if (packet.flags & 2 && !received_syn) {  // SYN flag
                printf("syn_packet : %d-%d\n", received_checksum, packet.flags);
                struct RUDP_Packet syn_ack_packet;
                memset(&syn_ack_packet, 0, sizeof(syn_ack_packet));
                syn_ack_packet.flags = 2 | 1;  // SYN-ACK flag
                syn_ack_packet.checksum = calculate_checksum(&syn_ack_packet);
                syn_ack_packet.length = htons(0);
                rudp_send(sockfd, &syn_ack_packet, receiver_addr);
                start_time = get_time_in_ms();
                received_syn = 1;
            } else if (packet.flags == 4) {  // FIN flag
                struct RUDP_Packet fin_ack_packet;
                memset(&fin_ack_packet, 0, sizeof(fin_ack_packet));
                fin_ack_packet.flags = 4 | 1;  // FIN-ACK flag
                fin_ack_packet.checksum = calculate_checksum(&fin_ack_packet);
                fin_ack_packet.length = htons(0);
                rudp_send(sockfd, &fin_ack_packet, receiver_addr);
                end_time = get_time_in_ms();

                double transfer_time = end_time - start_time;
                double speed = (total_received / (1024.0 * 1024.0)) / (transfer_time / 1000.0); // Speed in MB/s

                printf("File transfer completed.\n");
                printf("----------------------------------\n");
                printf("* Statistics *\n");
                printf("- Run #1 Data: Time=%.2fms; Speed=%.2fMB/s\n", transfer_time, speed);
                printf("- Average time: %.2fms\n", transfer_time);
                printf("- Average bandwidth: %.2fMB/s\n", speed);
                printf("----------------------------------\n");
                printf("Receiver end.\n");
            } else if(packet.flags == 8){
                printf("Connection terminated");
                break;
            } else {
                struct RUDP_Packet ack_packet;
                memset(&ack_packet, 0, sizeof(ack_packet));
                ack_packet.flags = 1;  // ACK flag
                ack_packet.length = packet.length;
                memcpy(ack_packet.data, packet.data, ntohs(packet.length));  // Copy data for identification
                ack_packet.checksum = calculate_checksum(&ack_packet);

                if (rudp_send(sockfd, &ack_packet, receiver_addr) < 0) {
                    fprintf(stderr, "Failed to send ACK\n");
                    exit(EXIT_FAILURE);
                }
                total_received += ntohs(packet.length);
            }
        } else {
            printf("Checksum mismatch, packet ignored\n");
        }
    }

    if (rudp_close(sockfd) < 0) {
        fprintf(stderr, "Error closing socket\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
