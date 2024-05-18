#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>

#define FILE_SIZE_BYTES (2 * 1024 * 1024)

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

int main(int argc, char *argv[]) {
    if (argc != 5 || strcmp(argv[1], "-ip") != 0 || strcmp(argv[3], "-p") != 0) {
        fprintf(stderr, "Usage: %s -ip <receiver_ip> -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *receiver_ip = argv[2];
    int port = atoi(argv[4]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failure");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = inet_addr(receiver_ip);
    receiver_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
        perror("Connect failure");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    size_t data_size = 2 * 1024 * 1024;

    size_t bytes_sent = 0;
    char choice;
    do {
        char *data = util_generate_random_data(data_size);
        bytes_sent = 0;
        while (bytes_sent < FILE_SIZE_BYTES) {
            ssize_t sent = send(sockfd, data + bytes_sent, FILE_SIZE_BYTES - bytes_sent, 0);
            if (sent < 0) {
                perror("Transmission failure");
                free(data);
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            bytes_sent += sent;
        }
        free(data);

        printf("\nSend another random file? (y/n): ");
        scanf(" %c", &choice);

        if (send(sockfd, &choice, sizeof(choice), 0) < 0) {
            perror("Send choice failure");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        printf("File sent successfully.\n");

    } while (choice == 'y' || choice == 'Y');

    if (send(sockfd, &choice, sizeof(choice), 0) < 0) {
        perror("Send choice failure");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    close(sockfd);
    return 0;
}
