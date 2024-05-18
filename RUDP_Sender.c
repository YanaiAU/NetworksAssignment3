#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/poll.h>
#include "RUDP_API.h"

#define MAX_RETRANSMISSION_ATTEMPTS 3
#define TIMEOUT 5
#define ACK_FLAG 1
#define SYN_FLAG 2
#define FIN_FLAG 4
#define START_FILE_FLAG 8

char *util_generate_random_data(unsigned int size) {
    char *buffer = NULL;
    if (size == 0)
        return NULL;
    buffer = (char *)calloc(size, sizeof(char));
    if (buffer == NULL)
        return NULL;
    srand(time(NULL));
    for (unsigned int i = 0; i < size; i++)
        *(buffer + i) = ((unsigned int)rand() % 256);
    return buffer;
}

struct SentPacket {
    struct RUDP_Packet packet;
    int retransmission_attempts;
};

int main(int argc, char *argv[]) {
    if (argc != 5 || strcmp(argv[1], "-ip" ) != 0 || strcmp(argv[3], "-p") != 0) {
        fprintf(stderr, "Usage: %s <receiver_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *receiver_ip = argv[2];
    int port = atoi(argv[4]);

    int sockfd = rudp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Error creating socket\n");
        exit(EXIT_FAILURE);
    }

    struct SentPacket sent_packets[100];
    memset(sent_packets, 0, sizeof(sent_packets));

    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = inet_addr(receiver_ip);
    receiver_addr.sin_port = htons(port);

    struct RUDP_Packet syn_packet;
    syn_packet.flags = SYN_FLAG;
    syn_packet.checksum = htons(0);
    syn_packet.length = htons(0);
    syn_packet.seq_num = 0;
    rudp_send(sockfd, &syn_packet, receiver_addr);
    printf("SYN packet sent.\n");

    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    int syn_ack_received = 0;

    while (!syn_ack_received) {
        int poll_ret = poll(fds, 1, TIMEOUT * 1000);
        if (poll_ret > 0 && (fds[0].revents & POLLIN)) {
            struct RUDP_Packet ack_packet;
            if (rudp_recv(sockfd, &ack_packet, &receiver_addr) > 0) {
                if ((ack_packet.flags & ACK_FLAG) && (ack_packet.flags & SYN_FLAG)) {
                    printf("SYN-ACK received.\n");
                    syn_ack_received = 1;
                }
            }
        } else {
            printf("Timeout waiting for SYN-ACK. Retrying...\n");
            rudp_send(sockfd, &syn_packet, receiver_addr);
        }
    }

    char choice;
    do {
        int cwnd = 1;
        int ssthresh = 64;
        int duplicate_acks = 0;
        int last_ack_received = -1;

        struct RUDP_Packet start_file_packet;
        start_file_packet.flags = START_FILE_FLAG;
        start_file_packet.checksum = htons(0);
        start_file_packet.length = htons(0);
        start_file_packet.seq_num = 0;
        rudp_send(sockfd, &start_file_packet, receiver_addr);

        size_t data_size = 2 * 1024 * 1024;
        char *data = util_generate_random_data(data_size);
        if (data == NULL) {
            fprintf(stderr, "Error generating random data\n");
            exit(EXIT_FAILURE);
        }

        size_t bytes_sent = 0;
        int seq_num = 0;

        while (bytes_sent < data_size) {
            size_t chunk_size = 1017;
            if (bytes_sent + chunk_size > data_size)
                chunk_size = data_size - bytes_sent;

            struct RUDP_Packet packet;
            packet.flags = 0;
            packet.checksum = htons(0);
            packet.length = htons(chunk_size);
            packet.seq_num = seq_num;
            memcpy(packet.data, data + bytes_sent, chunk_size);
            rudp_send(sockfd, &packet, receiver_addr);

            struct SentPacket sent_packet = {packet, time(NULL)};
            sent_packets[seq_num % 100] = sent_packet;
            sent_packet.retransmission_attempts = 0;

            int ack_received = 0;
            while (!ack_received) {
                int poll_ret = poll(fds, 1, TIMEOUT * 1000);
                if (poll_ret > 0 && (fds[0].revents & POLLIN)) {
                    struct RUDP_Packet ack_packet;
                    if (rudp_recv(sockfd, &ack_packet, &receiver_addr) > 0) {
                        if ((ack_packet.flags & ACK_FLAG) && (ack_packet.seq_num == seq_num)) {
                            bytes_sent += chunk_size;
                            seq_num++;
                            if (ack_packet.seq_num == last_ack_received) {
                                duplicate_acks++;
                                if (duplicate_acks == 3) {
                                    ssthresh = cwnd / 2;
                                    cwnd = ssthresh + 3;
                                    duplicate_acks = 0;
                                    printf("Fast retransmit\n");
                                    rudp_send(sockfd, &sent_packets[ack_packet.seq_num % 100].packet, receiver_addr);
                                }
                            } else {
                                last_ack_received = ack_packet.seq_num;
                                duplicate_acks = 0;
                                if (cwnd < ssthresh) {
                                    cwnd++;
                                } else {
                                    cwnd += 1 / cwnd;
                                }
                                ack_received = 1;
                            }
                        }
                    }
                } else {
                    printf("Timeout occurred! Retransmitting packet with seq_num %d\n", seq_num);
                    rudp_send(sockfd, &sent_packet.packet, receiver_addr);
                    sent_packet.retransmission_attempts++;
                    if (sent_packet.retransmission_attempts >= MAX_RETRANSMISSION_ATTEMPTS) {
                        fprintf(stderr, "Max retransmission attempts reached for packet %d\n", seq_num);
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }

        free(data);

        printf("\nSend another random file? (y/n): ");
        scanf(" %c", &choice);

    } while (choice == 'y' || choice == 'Y');

    struct RUDP_Packet fin_packet;
    memset(&fin_packet, 0, sizeof(fin_packet));
    fin_packet.flags = FIN_FLAG;
    fin_packet.checksum = htons(0);
    fin_packet.length = htons(0);
    fin_packet.seq_num = 0;
    rudp_send(sockfd, &fin_packet, receiver_addr);
    printf("FIN packet sent.\n");

    int fin_ack_received = 0;
    while (!fin_ack_received) {
        int poll_ret = poll(fds, 1, TIMEOUT * 1000);
        if (poll_ret > 0 && (fds[0].revents & POLLIN)) {
            struct RUDP_Packet ack_packet;
            if (rudp_recv(sockfd, &ack_packet, &receiver_addr) > 0) {
                if ((ack_packet.flags & ACK_FLAG) && (ack_packet.flags & FIN_FLAG)) {
                    printf("FIN-ACK received.\n");
                    fin_ack_received = 1;
                }
            }
        } else {
            printf("Timeout waiting for FIN-ACK. Retrying...\n");
            rudp_send(sockfd, &fin_packet, receiver_addr);
        }
    }

    if (rudp_close(sockfd) < 0) {
        fprintf(stderr, "Error closing socket\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
