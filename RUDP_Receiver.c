#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "RUDP_API.h"

#define ACK_FLAG 1
#define SYN_FLAG 2
#define FIN_FLAG 4
#define START_FILE_FLAG 8
#define FILE_SIZE_BYTES (2 * 1024 * 1024)

double get_time_in_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Starting Receiver...\n");

    int port = atoi(argv[2]);

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
    int seq_num = 0;
    char received_data[FILE_SIZE_BYTES];
    memset(received_data, 0, FILE_SIZE_BYTES);

    size_t total_received = 0;

    int file_count = 0;
    double total_time = 0.0;
    double total_speed = 0.0;
    double start_time, end_time;

    double transfer_times[100];
    double speeds[100];

    while (1) {
        struct RUDP_Packet packet;
        int bytes_received = rudp_recv(sockfd, &packet, &receiver_addr);
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
                rudp_send(sockfd, &syn_ack_packet, receiver_addr);
                received_syn = 1;
                printf("Sender connected.\n");
            } else if (packet.flags & START_FILE_FLAG) {
                printf("Beginning to receive file...\n");
                start_time = get_time_in_ms();
                total_received = 0;
                seq_num = 0;
            } else if (packet.flags & FIN_FLAG) {
                struct RUDP_Packet fin_ack_packet;
                memset(&fin_ack_packet, 0, sizeof(fin_ack_packet));
                fin_ack_packet.flags = FIN_FLAG | ACK_FLAG;
                fin_ack_packet.checksum = htons(0);
                fin_ack_packet.length = htons(0);
                fin_ack_packet.seq_num = 0;
                rudp_send(sockfd, &fin_ack_packet, receiver_addr);
                break;
            } else if (packet.seq_num == seq_num) {
                memcpy(received_data + (seq_num * ntohs(packet.length)), packet.data, ntohs(packet.length));

                struct RUDP_Packet ack_packet;
                memset(&ack_packet, 0, sizeof(ack_packet));
                ack_packet.flags = ACK_FLAG;
                ack_packet.checksum = htons(0);
                ack_packet.length = htons(0);
                ack_packet.seq_num = packet.seq_num;
                rudp_send(sockfd, &ack_packet, receiver_addr);
                total_received += ntohs(packet.length);
                seq_num++;
            }
        }

        if (total_received >= FILE_SIZE_BYTES) {
            end_time = get_time_in_ms();
            double transfer_time = end_time - start_time;
            double speed = (total_received / (1024.0 * 1024.0)) / (transfer_time / 1000.0);

            transfer_times[file_count] = transfer_time;
            speeds[file_count] = speed;

            total_time += transfer_time;
            total_speed += speed;
            file_count++;

            printf("File received\n");
        }
    }
    if (file_count > 0) {
        double avg_time = total_time / file_count;
        double avg_speed = total_speed / file_count;

        printf("----------------------------------\n");
        printf("* Statistics *\n");
        for (int i = 0; i < file_count; i++) {
            printf("- Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", i + 1, transfer_times[i], speeds[i]);
        }
        printf("- Average time: %.2fms\n", avg_time);
        printf("- Average bandwidth: %.2fMB/s\n", avg_speed);
        printf("----------------------------------\n");
    }


    if (rudp_close(sockfd) < 0) {
        fprintf(stderr, "Error closing socket\n");
        exit(EXIT_FAILURE);
    }

    printf("Receiver end.\n");

    return 0;
}
