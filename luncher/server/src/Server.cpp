#include "Server.hpp"

void* flush_thread_func(void* arg)
{
    while (1) {
        fflush(stdout);
        fflush(stderr);
        usleep(500000); // 0.5 secunde = 500.000 microsecunde
    }
    return NULL;
}

void start_flusher_thread()
{
    pthread_t flush_thread;
    int       ret = pthread_create(&flush_thread, NULL, flush_thread_func, NULL);
    if (ret != 0) {
        perror("[ERROR] pthread_create (flush thread)");
    } else {
        pthread_detach(flush_thread); // rulează în fundal fără să aștepți
    }
}

void send_client_ip(int sockfd)
{
    struct client_info* ci;

    for (int i = 0; i < client_list.no_clients; ++i) {
        if (client_list.list[i].sockfd == sockfd) {
            ci = &client_list.list[i];
            break;
        }
    }

    send_message(sockfd, (void*) ci->clientIP, sizeof(ci->clientIP));
}

void send_start_signal(int sockfd)
{
    int ret;
    ret = send_message(sockfd, START, strlen(START));
}

void* handle_client_connection(void* arg)
{
    int  client_fd            = *(int*) arg;
    char buffer[MAX_MSG_SIZE] = {0};
    int  ret;

    send_client_ip(client_fd);
    send_start_signal(client_fd);

    while (1) {
        memset(buffer, 0, MAX_MSG_SIZE);
        ret = recv_message(client_fd, buffer);
        printf("[INFO] Message: %s\n", buffer);

        if (strncmp(buffer, CRASH, strlen(CRASH)) == 0) {
            pthread_mutex_lock(&_notificatioMutex);
            _notificationMessage = RESTART;
            _notification        = true;
            pthread_cond_signal(&_notificationCond);
            pthread_mutex_unlock(&_notificatioMutex);
        }

        sleep(1);
    }

    return NULL;
}

void* listen_thread_func(void* arg)
{
    int                listen_fd = *(int*) arg;
    int                client_fd = 0;
    struct sockaddr_in client_address;
    socklen_t          client_len = sizeof(client_address);
    pthread_t          client_thread;
    int                ret;

    printf("[INFO] Listen thread started. Listening...\n");

    while (1) {
        memset(&client_address, 0, sizeof(client_address));
        client_fd = accept(listen_fd, (struct sockaddr*) &client_address, &client_len);
        if (client_fd < 0) {
            perror("[ERROR] accept()");
            return NULL;
        }

        printf("[INFO] New connection %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        addNewClient(client_fd, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), &client_list);
        client_fd_list.push_back(client_fd);

        printf("[INFO] Client added to client list!\n");

        pthread_t client_thread;
        ret = pthread_create(&client_thread, NULL, handle_client_connection, (int*) &client_fd);
        if (ret != 0) {
            perror("[ERROR] pthread_create()");
            return NULL;
        }
        pthread_detach(client_thread);
    }
    return NULL;
}

ssize_t send_message(int sockfd, const void* buffer, size_t len)
{
    int   ret;
    int   bytes_sent, total_bytes;
    char* buff = (char*) &len;

    bytes_sent  = 0;
    total_bytes = sizeof(len);

    while (total_bytes > bytes_sent) {
        ret = send(sockfd, buff + bytes_sent, total_bytes - bytes_sent, 0);
        if (ret < 0) {
            perror("[ERROR] send() length");
            exit(EXIT_FAILURE);
        }
        bytes_sent += ret;
    }

    buff        = (char*) buffer;
    bytes_sent  = 0;
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

ssize_t recv_message(int sockfd, void* buffer)
{
    int    ret;
    int    bytes_received = 0;
    int    total_bytes    = sizeof(size_t);
    size_t len;
    char*  buff = (char*) &len;

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
    total_bytes    = len;
    buff           = (char*) buffer;

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
    static int         sockfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t          addr_len = sizeof(client_addr);
    pthread_t          listen_thread;
    int                ret;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[ERROR] socket()");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(LISTEN_PORT);

    if (bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("[ERROR] bind()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 10) < 0) {
        perror("[ERROR] listen()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&listen_thread, NULL, listen_thread_func, (int*) &sockfd);
    if (ret != 0) {
        perror("[ERROR] pthread_create()");
        exit(EXIT_FAILURE);
    }
    pthread_detach(listen_thread);

    return sockfd;
}

void stop_process(pid_t pid)
{
    int status;
    if (kill(pid, SIGKILL) == 0) {
        printf("Process %d was terminated.\n", pid);
    } else {
        printf("Failed to terminate process %d.\n", pid);
    }

    if (waitpid(pid, &status, 0) < 0) {
        printf("[ERROR] Error waiting child process...\n");
    } else {
        if (WIFEXITED(status)) {
            printf("[INFO] Child exited normally with code %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[INFO] Child was terminated by signal %d\n", WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("[INFO] Child was stopped by signal %d\n", WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            printf("[INFO] Child was resumed by SIGCONT\n");
        } else {
            printf("[INFO] Unknown child status: %d\n", status);
        }
    }
}

int main(int argc, char* argv[])
{
    int                  ret = 0;
    std::optional<pid_t> proxyPid;

    start_flusher_thread();

    cm = utils::ConfigurationManager(argv[1]);
    if (cm.parse() != true) {
        printf("[ERROR] utils::ConfigurationManager::parse()\n");
        exit(-1);
    }
    printf("[INFO] Configuration loaded!\n");

    init_server();
    printf("[INFO] Server launcher started...\n");

    em = utils::ExecutionManager(cm);

    utils::EntityConfig proxyConfig = cm.getFuzzer();

    proxyPid = em.launchEntity(proxyConfig, -1);

    while (1) {
        pthread_mutex_lock(&_notificatioMutex);
        while (_notification == false) {
            pthread_cond_wait(&_notificationCond, &_notificatioMutex);
        }

        printf("[INFO] Notification received...\n");
        if (_notificationMessage == RESTART) {
            stop_process(proxyPid.value());
            proxyPid = em.launchEntity(proxyConfig, -1);
            printf("[INFO] Proxy restarted...\n");
            for (auto& clientfd : client_fd_list) {
                send_message(clientfd, RESTART, sizeof(RESTART));
                printf("\t[INFO] Sent RESTART message to client...\n");
            }
            _notificationMessage.clear();
            _notification = false;
            pthread_mutex_unlock(&_notificatioMutex);
        }
    };
    // TODO: De revizuit commander.py pentru a porni serverul care va porni proxiul si clientul care va porni unul din
    // capete
    // TODO: De verificat logica de logare
    // TODO: De implementat logica de fuzzing
    // TODO: De implementat mai multe moduri de rulare.
    // TODO: MAI VINO CU IDEI.
    return 0;
}
