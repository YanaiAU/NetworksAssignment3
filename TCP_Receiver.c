#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failure");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    receiver_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0) {
        perror("Bind failure");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 5) < 0) {
        perror("Listen failure");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char received_data[FILE_SIZE_BYTES];
    memset(received_data, 0, FILE_SIZE_BYTES);

    size_t total_received = 0;

    int file_count = 0;
    double total_time = 0.0;
    double total_speed = 0.0;
    double start_time, end_time;

    double transfer_times[100];
    double speeds[100];

    int received_n = 1;
    while (received_n) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (newsockfd < 0) {
            perror("Accept failure");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        while (1) {
            total_received = 0;
            start_time = get_time_in_ms();

            while (total_received < FILE_SIZE_BYTES) {
                int bytes_received = recv(newsockfd, received_data + total_received, FILE_SIZE_BYTES - total_received, 0);
                if (bytes_received < 0) {
                    perror("Receive failure");
                    close(newsockfd);
                    close(sockfd);
                    exit(EXIT_FAILURE);
                } else if (bytes_received == 0) {
                    break;
                }
                total_received += bytes_received;
            }

            end_time = get_time_in_ms();
            double transfer_time = end_time - start_time;
            double speed = (total_received / (1024.0 * 1024.0)) / (transfer_time / 1000.0);

            transfer_times[file_count] = transfer_time;
            speeds[file_count] = speed;

            total_time += transfer_time;
            total_speed += speed;
            file_count++;

            printf("File received\n");

            char continue_flag;
            int flag_received = recv(newsockfd, &continue_flag, sizeof(continue_flag), 0);
            if (flag_received < 0 || continue_flag == 'n' || continue_flag == 'N') {
                received_n = 0;
                break;
            }

            if (file_count >= 100) {
                break;
            }
        }

        close(newsockfd);

        if (file_count >= 100) {
            break;
        }
    }

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

    close(sockfd);

    printf("Receiver end.\n");

    return 0;
}
