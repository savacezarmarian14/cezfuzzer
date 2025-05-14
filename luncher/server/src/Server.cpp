#include "Server.hpp"


void *handle_client_connection(void *arg)
{
    int client_fd = *(int *) arg;
    char buffer[MAX_MSG_SIZE] = {0};
    int ret;

    while (1) {
        ret = recv_message(client_fd, buffer);
        printf("[INFO] Message: %s\n", buffer);
        sleep(1);
        //printf("[INFO] Recieved message: %s\n", buffer);
    }

    return NULL;
}

void *listen_thread_func(void *arg)
{
    int listen_fd = *(int *)arg;
    int client_fd = 0;
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    pthread_t client_thread;
    int ret;

    printf("[INFO] Listen thread started. Listening...\n");

    while (1) {
        memset(&client_address, 0, sizeof(client_address));
        client_fd = accept(listen_fd, (struct sockaddr *) &client_address, &client_len);
        if (client_fd < 0) {
            perror("[ERROR] accept()");
            return NULL;
        }

        printf("[INFO] New connection %s:%d\n",
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port));
        
        pthread_t client_thread;
        ret = pthread_create(&client_thread, NULL, handle_client_connection, (int *) &client_fd);
        if (ret != 0) {
            perror("[ERROR] pthread_create()");
            return NULL;
        }
        pthread_detach(client_thread);
    }
    return NULL;
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

ssize_t recv_message(int sockfd, void *buffer)
{
    int ret;
    int bytes_received = 0;
    int total_bytes = sizeof(size_t);
    size_t len;
    char *buff = (char *) &len;

    // 1. Primește lungimea
    while (bytes_received < total_bytes) {
        ret = recv(sockfd, buff + bytes_received, total_bytes - bytes_received, 0);
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
    buff = (char *) buffer;

    while (bytes_received < total_bytes) {
        ret = recv(sockfd, buff + bytes_received, total_bytes - bytes_received, 0);
        if (ret <= 0) {
            perror("[ERROR] recv() message");
            return -1;
        }
        bytes_received += ret;
    }

    return len;
}

int init_server()
{
    int sockfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t listen_thread;
    int ret;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[ERROR] socket()");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(LISTEN_PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[ERROR] bind()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 10) < 0) {
        perror("[ERROR] listen()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&listen_thread, NULL, listen_thread_func, (int *) &sockfd);
    if (ret != 0) {
        perror("[ERROR] pthread_create()");
        exit(EXIT_FAILURE);
    }
    pthread_detach(listen_thread);

    return sockfd;

}

int main(int argc, char *argv[]) {
    init_server();
    config = YAML::Load(CONFIG_FILE);

    while(1);
    // TODO: De revizuit commander.py pentru a porni serverul care va porni proxiul si clientul care va porni unul din capete
    // TODO: De verificat logica de logare
    // TODO: De implementat logica de fuzzing
    // TODO: De implementat mai multe moduri de rulare.
    // TODO: MAI VINO CU IDEI.
    return 0;
}
