#include "Client.hpp"

#define LISTEN_PORT 23927

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
        pthread_detach(flush_thread);
    }
}

void set_entities_list(int sockfd)
{
    char buffer[64] = {0};
    int  ret;

    ret = recv_message(sockfd, (void*) buffer);
    strcpy(IP, buffer);
    printf("[INFO] Client IP: %s. Setting entities list...\n", buffer);

    entities = cm.getEntities(buffer);

    for (auto& entity : entities) {
        printf("[INFO] entity: %s\n", entity.name.c_str());
    }
}

void analyze_child_exit_status(int status)
{
    if (WIFEXITED(status)) {
        printf("Child exited with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Child terminated by signal: %s (signal %d)\n", strsignal(sig), sig);

        switch (sig) {
            case SIGSEGV:
                printf("Segmentation fault (possible stack overflow or invalid memory access).\n");
                break;
            case SIGFPE:
                printf("Floating point exception (e.g., divide by zero).\n");
                break;
            case SIGABRT:
                printf("Abort signal (e.g., assert failure).\n");
                break;
            default:
                printf("Other signal.\n");
        }
    } else {
        printf("Child terminated in an unknown way.\n");
    }
}

void* monitor_child(void* args)
{
    struct _monitor_child_struct {
        pid_t pid;
        int   sockfd;
    }*    _threadArg = (struct _monitor_child_struct*) args;
    pid_t _pid       = _threadArg->pid;
    int   _sockfd    = _threadArg->sockfd;
    int   status     = 0;
    char  buffer[16] = CRASH;

    free(_threadArg);

    waitpid(_pid, &status, 0);

    analyze_child_exit_status(status);
    send_message(_sockfd, buffer, strlen(buffer));

    return NULL;
}

void start_entities(int sockfd)
{
    struct _monitor_child_struct {
        pid_t pid;
        int   sockfd;
    }*         _threadArg = NULL;
    static int index      = 0;
    index++;
    for (auto& entity : entities) {
        std::optional<pid_t> pid = em.launchEntity(entity, index);
        if (pid.has_value()) {
            processList.push_back(pid.value());
            _threadArg         = (struct _monitor_child_struct*) malloc(1 * sizeof(struct _monitor_child_struct));
            _threadArg->pid    = pid.value();
            _threadArg->sockfd = sockfd;
            pthread_t thread;
            pthread_create(&thread, NULL, monitor_child, (void*) _threadArg);
            pthread_detach(thread);
            printf("[DEBUG] Created waiting thread for application %d\n", pid.value());
        }
    }
}

void stop_process(pid_t pid)
{
    if (kill(pid, SIGKILL) == 0) {
        printf("Process %d was terminated.\n", pid);
    } else {
        printf("Failed to terminate process %d.\n", pid);
    }
}

void stop_processes()
{
    for (pid_t pid : processList) {
        stop_process(pid);
    }
    processList.clear();
}

void multiplex_message(char* buffer, int sockfd)
{
    if (strncmp(buffer, START, sizeof(START)) == 0) {
        printf("[INFO] Starting Entities...\n");
        start_entities(sockfd);
    } else if (strncmp(buffer, RESTART, sizeof(RESTART)) == 0) {
        stop_processes();
        printf("[INFO] Restarting Entities...\n");
        start_entities(sockfd);
    }
}

void client_loop(int sockfd)
{
    int  ret;
    char buffer[MAX_MSG_SIZE];
    set_entities_list(sockfd);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ret = recv_message(sockfd, buffer);
        multiplex_message(buffer, sockfd);
    }
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

    while (bytes_received < total_bytes) {
        ret = recv(sockfd, buff + bytes_received, total_bytes - bytes_received, 0);
        if (ret <= 0) {
            perror("[ERROR] recv() length");
            return -1;
        }
        bytes_received += ret;
    }

    if (len > MAX_MSG_SIZE) {
        fprintf(stderr, "[ERROR] mesaj prea mare (%zu bytes)\n", len);
        return -1;
    }

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

int init_client(char* serverIP)
{
    int                sockfd = 0;
    struct sockaddr_in server_addr;
    socklen_t          server_len = sizeof(server_addr);
    int                ret;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[ERROR] socket()");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(serverIP);
    server_addr.sin_port        = htons(LISTEN_PORT);

    ret = connect(sockfd, (struct sockaddr*) &server_addr, server_len);
    if (ret < 0) {
        perror("[ERROR] connect()");
        exit(EXIT_FAILURE);
    }

    printf("[INFO] Connection established! Start transfering data...\n");
    (sockfd);

    return sockfd;
}

int main(int argc, char* argv[])
{
    char* server_addr = argv[1];

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