#include "Client.hpp"

#define LISTEN_PORT 23927

void test_function(int sockfd)
{
    int ret;

    while (1) {
        ret = send_message(sockfd, "Hello", 5);
        printf("[INFO] Message sent.\n");
        sleep(1);
    }
}

ssize_t send_message(int sockfd, const void *buffer, size_t len)
{
    int ret;
    int bytes_sent, total_bytes;
    char *buff = (char *) &len;

    bytes_sent = 0;
    total_bytes = sizeof(len);

    while (total_bytes > bytes_sent) {
        ret = send(sockfd, buff + bytes_sent, total_bytes - bytes_sent, 0);
        if (ret < 0) {
            perror("[ERROR] send() length");
            exit(EXIT_FAILURE);
        }
        bytes_sent += ret;
    }

    buff = (char *) buffer;
    bytes_sent = 0;
    total_bytes = len;

    while (total_bytes > bytes_sent) {
        ret = send(sockfd, buff + bytes_sent, total_bytes - bytes_sent, 0);
        if (ret < 0) {
            perror("[ERROR] send() message");
            exit(EXIT_FAILURE);
        }
        bytes_sent += ret;
    }

    return len;
}

ssize_t recv_message(int sockfd, char *buffer)
{
    int ret;
    int bytes_received = 0;
    int total_bytes = sizeof(size_t);
    size_t len;
    char *len_ptr = (char *) &len;

    // 1. Primește lungimea
    while (bytes_received < total_bytes) {
        ret = recv(sockfd, len_ptr + bytes_received, total_bytes - bytes_received, 0);
        if (ret <= 0) {
            perror("[ERROR] recv() length");
            return -1;
        }
        bytes_received += ret;
    }

    // 2. Verifică dacă e peste limită
    if (len > MAX_MSG_SIZE) {
        fprintf(stderr, "[ERROR] mesaj prea mare (%zu bytes)\n", len);
        return -1;
    }

    // 3. Primește mesajul în bufferul static
    bytes_received = 0;
    total_bytes = len;

    while (bytes_received < total_bytes) {
        ret = recv(sockfd, buffer + bytes_received, total_bytes - bytes_received, 0);
        if (ret <= 0) {
            perror("[ERROR] recv() message");
            return -1;
        }
        bytes_received += ret;
    }

    return len;
}

int init_client(char* serverIP)
{
    int sockfd = 0;
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    int ret;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[ERROR] socket()");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(serverIP);
    server_addr.sin_port = htons(LISTEN_PORT);

    ret = connect(sockfd, (struct sockaddr *)&server_addr, server_len);
    if (ret < 0) {
        perror("[ERROR] connect()");
        exit(EXIT_FAILURE);
    }

    printf("[INFO] Connection established! Start transfering data...\n");
    test_function(sockfd);

    return sockfd;
}

int main(int argc, char *argv[])
{
    init_client(argv[1]);
    
    return 0;
}