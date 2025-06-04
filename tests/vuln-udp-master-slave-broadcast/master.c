// master_broadcast.c
//
// Master process listens on MASTER_PORT (7000).
// It accepts messages "MSG:<slave_id>:<text>" from any slave.
// Then it rebroadcasts "BROADCAST:<slave_id>:<text>" to all known slave ports.
// Slave list is hardcoded for simplicity. Parsing is fragile.

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MASTER_PORT 7000
#define BUF_SIZE    65536
#define MAX_SLAVES  3
const int SLAVE_PORTS[MAX_SLAVES] = {7101, 7102, 7103};

int main()
{
    int                sock;
    struct sockaddr_in master_addr, from_addr, slave_addr;
    char               buffer[BUF_SIZE];
    socklen_t          from_len = sizeof(from_addr);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[master] socket failed");
        return 1;
    }

    memset(&master_addr, 0, sizeof(master_addr));
    master_addr.sin_family      = AF_INET;
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    master_addr.sin_port        = htons(MASTER_PORT);

    if (bind(sock, (struct sockaddr*) &master_addr, sizeof(master_addr)) < 0) {
        perror("[master] bind failed");
        close(sock);
        return 1;
    }

    printf("[master] listening on port %d\n", MASTER_PORT);

    while (1) {
        // Receive from any slave
        ssize_t len = recvfrom(sock, buffer, BUF_SIZE - 1, 0, (struct sockaddr*) &from_addr, &from_len);
        if (len < 0) {
            perror("[master] recvfrom failed");
            continue;
        }
        buffer[len] = '\0';

        // Expect "MSG:<slave_id>:<text>"
        char* tok1     = strtok(buffer, ":");
        char* slave_id = NULL;
        char* text     = NULL;

        if (tok1 && strcmp(tok1, "MSG") == 0) {
            slave_id = strtok(NULL, ":");
            text     = strtok(NULL, "\0"); // rest of buffer
        }

        if (!slave_id || !text) {
            printf("[master] malformed message: %.*s\n", (int) len, buffer);
            continue;
        }

        // Build broadcast message
        char reply[BUF_SIZE];
        int  rlen = snprintf(reply, BUF_SIZE, "BROADCAST:%s:%s", slave_id, text);
        if (rlen < 0)
            rlen = 0;

        // Send to all slaves
        for (int i = 0; i < MAX_SLAVES; ++i) {
            memset(&slave_addr, 0, sizeof(slave_addr));
            slave_addr.sin_family      = AF_INET;
            slave_addr.sin_addr.s_addr = htonl(INADDR_ANY); // loopback for demo
            slave_addr.sin_port        = htons(SLAVE_PORTS[i]);

            sendto(sock, reply, rlen, 0, (struct sockaddr*) &slave_addr, sizeof(slave_addr));
        }
        printf("[master] broadcast from Slave '%s': %s\n", slave_id, text);
    }

    close(sock);
    return 0;
}
