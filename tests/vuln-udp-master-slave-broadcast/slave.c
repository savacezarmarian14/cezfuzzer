// slave_broadcast.c
//
// A slave process that listens on a fixed port (one of 7101, 7102, 7103).
// Usage: slave_broadcast <slave_id> <my_port> <master_ip>
// Sends a "PING_MASTER:<slave_id>" every 10 seconds (optional).
// Reads any "BROADCAST:<slave_id>:<text>" and prints it.

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MASTER_PORT 7000
#define BUF_SIZE    65536

char        SLAVE_ID[32];
int         MY_PORT;
const char* MASTER_IP;

void* ping_master(void* arg)
{
    int                sock;
    struct sockaddr_in master_addr;
    char               pingmsg[256];
    socklen_t          addrlen = sizeof(master_addr);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[slave] ping socket failed");
        return NULL;
    }

    memset(&master_addr, 0, sizeof(master_addr));
    master_addr.sin_family = AF_INET;
    master_addr.sin_port   = htons(MASTER_PORT);
    inet_pton(AF_INET, MASTER_IP, &master_addr.sin_addr);

    while (1) {
        int plen = snprintf(pingmsg, sizeof(pingmsg), "PING_MASTER:%s", SLAVE_ID);
        sendto(sock, pingmsg, plen, 0, (struct sockaddr*) &master_addr, addrlen);
        sleep(10);
    }

    close(sock);
    return NULL;
}

int main(int argc, char** argv)
{
    if (argc != 4) {
        printf("Usage: %s <slave_id> <my_port> <master_ip>\n", argv[0]);
        return 1;
    }
    strncpy(SLAVE_ID, argv[1], sizeof(SLAVE_ID) - 1);
    MY_PORT   = atoi(argv[2]);
    MASTER_IP = argv[3];

    int                sock;
    struct sockaddr_in my_addr;
    char               buffer[BUF_SIZE];
    socklen_t          addrlen = sizeof(my_addr);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[slave] socket failed");
        return 1;
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family      = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port        = htons(MY_PORT);

    if (bind(sock, (struct sockaddr*) &my_addr, sizeof(my_addr)) < 0) {
        perror("[slave] bind failed");
        close(sock);
        return 1;
    }

    printf("[slave %s] listening on port %d\n", SLAVE_ID, MY_PORT);

    // Spawn a thread to send periodic PING_MASTER
    pthread_t tid;
    pthread_create(&tid, NULL, ping_master, NULL);

    // Also send one MSG to master once at startup
    {
        int                msock;
        struct sockaddr_in master_addr;
        socklen_t          mlen = sizeof(master_addr);
        char               msg[BUF_SIZE];

        msock = socket(AF_INET, SOCK_DGRAM, 0);
        memset(&master_addr, 0, sizeof(master_addr));
        master_addr.sin_family = AF_INET;
        master_addr.sin_port   = htons(MASTER_PORT);
        inet_pton(AF_INET, MASTER_IP, &master_addr.sin_addr);

        int mlen2 = snprintf(msg, BUF_SIZE, "MSG:%s:Hello from slave %s", SLAVE_ID, SLAVE_ID);
        sendto(msock, msg, mlen2, 0, (struct sockaddr*) &master_addr, mlen);

        close(msock);
    }

    // Listen for any BROADCAST messages
    while (1) {
        ssize_t len = recvfrom(sock, buffer, BUF_SIZE - 1, 0, NULL, NULL);
        if (len < 0) {
            perror("[slave] recvfrom failed");
            break;
        }
        buffer[len] = '\0';
        // Expect "BROADCAST:<slave_id>:<text>"
        char* tok1 = strtok(buffer, ":");
        char* sid  = NULL;
        char* txt  = NULL;
        if (tok1 && strcmp(tok1, "BROADCAST") == 0) {
            sid = strtok(NULL, ":");
            txt = strtok(NULL, "\0");
        }
        if (sid && txt) {
            printf("[slave %s] got broadcast from %s: %s\n", SLAVE_ID, sid, txt);
        } else {
            printf("[slave %s] malformed broadcast: %.*s\n", SLAVE_ID, (int) len, buffer);
        }
    }

    close(sock);
    return 0;
}
