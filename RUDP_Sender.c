#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <sys/poll.h>
#include "RUDP_API.h"

#define MAX_RETRANSMISSION_ATTEMPTS 3
#define TIMEOUT 5
#define START_FILE_FLAG 8
#define SYN_FLAG 2
#define ACK_FLAG 1
#define FIN_FLAG 4

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
    time_t send_time;
    int retransmission_attempts;
    int ack_received;
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <receiver_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *receiver_ip = argv[1];
    int port = atoi(argv[2]);

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

    // Send SYN packet to establish connection
    struct RUDP_Packet syn_packet;
    syn_packet.flags = SYN_FLAG;
    syn_packet.checksum = 0;
    syn_packet.length = htons(0);
    rudp_send(sockfd, &syn_packet, receiver_addr);
    printf("syn_packet : %d-%d\n", syn_packet.checksum, syn_packet.flags);

    // Wait for SYN-ACK packet
    int syn_ack_received = 0;
    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    while (!syn_ack_received) {
        int poll_ret = poll(fds, 1, TIMEOUT * 1000);
        if (poll_ret > 0) {
            if (fds[0].revents & POLLIN) {
                struct RUDP_Packet ack_packet;
                if (rudp_recv(sockfd, &ack_packet, &receiver_addr) > 0) {
                    if ((ack_packet.flags & ACK_FLAG) && (ack_packet.flags & SYN_FLAG)) { // Check for ACK and SYN flags
                        printf("SYN-ACK received.\n");
                        syn_ack_received = 1;
                    }
                }
            }
        } else {
            printf("Timeout waiting for SYN-ACK.\n");
            exit(EXIT_FAILURE);
        }
    }

    char choice;
    do {
        // Send START_FILE packet
        struct RUDP_Packet start_file_packet;
        start_file_packet.flags = START_FILE_FLAG;
        start_file_packet.checksum = 0;
        start_file_packet.length = htons(0);
        rudp_send(sockfd, &start_file_packet, receiver_addr);

        size_t data_size = 2 * 1024 * 1024; // 2 MB
        char *data = util_generate_random_data(data_size);
        if (data == NULL) {
            fprintf(stderr, "Error generating random data\n");
            exit(EXIT_FAILURE);
        }

        size_t bytes_sent = 0;
        int num_packets_sent = 0;

        while (bytes_sent < data_size) {
            size_t chunk_size = 1019;
            if (bytes_sent + chunk_size > data_size)
                chunk_size = data_size - bytes_sent;

            struct RUDP_Packet packet;
            packet.checksum = 0;
            packet.flags = 0;
            packet.length = htons(chunk_size);
            memcpy(packet.data, data + bytes_sent, chunk_size);

            rudp_send(sockfd, &packet, receiver_addr);

            struct SentPacket sent_packet = {packet, time(NULL), 0, 0};
            sent_packets[num_packets_sent++] = sent_packet;

            bytes_sent += chunk_size;

            int received_ack = 0;
            while (!received_ack) {
                struct pollfd fds[1];
                fds[0].fd = sockfd;
                fds[0].events = POLLIN;

                int poll_ret = poll(fds, 1, TIMEOUT * 1000);
                if (poll_ret > 0) {
                    if (fds[0].revents & POLLIN) {
                        struct RUDP_Packet ack_packet;
                        if (rudp_recv(sockfd, &ack_packet, &receiver_addr) > 0) {
                            if (ack_packet.flags & ACK_FLAG) { // ACK flag
                                for (int i = 0; i < num_packets_sent; i++) {
                                    if (memcmp(&sent_packets[i].packet.data, ack_packet.data, ntohs(ack_packet.length)) == 0) {
                                        sent_packets[i].ack_received = 1;
                                        received_ack = 1;
                                        for (int j = i; j < num_packets_sent - 1; j++) {
                                            sent_packets[j] = sent_packets[j + 1];
                                        }
                                        num_packets_sent--;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    printf("Timeout occurred! No data received.\n");
                    break;
                }
            }

            time_t current_time = time(NULL);
            for (int i = 0; i < num_packets_sent; i++) {
                if (sent_packets[i].ack_received == 0 && (current_time - sent_packets[i].send_time) >= TIMEOUT) {
                    printf("Retransmitting packet with checksum %d\n", sent_packets[i].packet.checksum);
                    rudp_send(sockfd, &sent_packets[i].packet, receiver_addr);
                    sent_packets[i].send_time = current_time;
                    sent_packets[i].retransmission_attempts++;
                    if (sent_packets[i].retransmission_attempts >= MAX_RETRANSMISSION_ATTEMPTS) {
                        fprintf(stderr, "Max retransmission attempts reached for packet\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
        free(data);

        printf("\nSend another random file? (y/n): ");
        scanf(" %c", &choice);

    } while (choice == 'y' || choice == 'Y');

    // Send FIN packet
    struct RUDP_Packet fin_packet;
    memset(&fin_packet, 0, sizeof(fin_packet));
    fin_packet.flags = FIN_FLAG; // FIN flag
    fin_packet.checksum = 0;
    fin_packet.length = htons(0);
    rudp_send(sockfd, &fin_packet, receiver_addr);
    printf("FIN packet sent.\n");

    // Wait for acknowledgment of FIN packet
    int fin_ack_received = 0;
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    while (!fin_ack_received) {
        int poll_ret = poll(fds, 1, TIMEOUT * 1000); // Wait for ACK with a longer timeout
        if (poll_ret > 0) {
            if (fds[0].revents & POLLIN) {
                struct RUDP_Packet ack_packet;
                if (rudp_recv(sockfd, &ack_packet, &receiver_addr) > 0) {
                    if ((ack_packet.flags & ACK_FLAG) && (ack_packet.flags & FIN_FLAG)) { // Check for ACK and FIN flags
                        printf("FIN ACK received.\n");
                        fin_ack_received = 1;
                    }
                }
            }
        } else {
            printf("Timeout waiting for FIN ACK.\n");
            break;
        }
    }

    if (rudp_close(sockfd) < 0) {
        fprintf(stderr, "Error closing socket\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
