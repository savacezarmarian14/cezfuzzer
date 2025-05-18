#include "Client.hpp"

#define LISTEN_PORT 23927

void* flush_thread_func(void* arg) {
    while (1) {
        fflush(stdout);
        fflush(stderr);
        usleep(500000); // 0.5 secunde = 500.000 microsecunde
    }
    return NULL;
}

void start_flusher_thread() {
    pthread_t flush_thread;
    int ret = pthread_create(&flush_thread, NULL, flush_thread_func, NULL);
    if (ret != 0) {
        perror("[ERROR] pthread_create (flush thread)");
    } else {
        pthread_detach(flush_thread); // rulează în fundal fără să aștepți
    }
}

void set_entities_list(int sockfd)
{
    char buffer[64] = {0};
    int ret;

    ret = recv_message(sockfd, (void *)buffer);    
    strcpy(IP, buffer);
    printf("[INFO] Client IP: %s. Setting entities list...\n", buffer);

    entities = cm.getEntities(buffer);

    for (auto& entity : entities) {
        printf("[INFO] entity: %s\n", entity.name.c_str());
    }
}

void start_entities()
{
    for (auto& entity : entities) {
        em.launchEntity(entity);
    }
}
void multiplex_message(char *buffer)
{
    if (strncmp(buffer, START, sizeof(START)) == 0) {
        printf("[INFO] Starting Entities...\n");
        start_entities();
    }
}

void client_loop(int sockfd)
{
    int ret;
    char buffer[MAX_MSG_SIZE];
    set_entities_list(sockfd);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ret = recv_message(sockfd, buffer);
        multiplex_message(buffer);
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
     (sockfd);

     


    return sockfd;
}

int main(int argc, char *argv[])
{
    char *server_addr = argv[1];

    start_flusher_thread();

    cm = utils::ConfigurationManager(argv[2]);
    if (cm.parse() != true) {
        printf("[ERROR] utils::ConfigurationManager::parse()\n");
        exit(-1);
    }
    printf("[INFO] Configuration loaded!\n");

    em = utils::ExecutionManager(cm);
    printf("[INFO] Execution Manager loaded!\n");

    int sockfd = init_client(server_addr);
    client_loop(sockfd);

    printf("[INFO] Client launcher started...");
    
    return 0;
}