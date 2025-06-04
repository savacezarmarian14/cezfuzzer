// pingpong_player.c
//
// A single program that can act as either Player A or Player B.
// Usage: pingpong_player <A|B> <peer_ip>
// If role is 'A', bind to 6000 and send first "PING:0" to peer port 6001.
// If role is 'B', bind to 6001 and wait for PING from A, then reply with PONG.
// Continues incrementing counter until receiving "STOP".

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT_A   6000
#define PORT_B   6001
#define BUF_SIZE 4096

int main(int argc, char** argv)
{
    if (argc != 3) {
        printf("Usage: %s <A|B> <peer_ip>\n", argv[0]);
        return 1;
    }

    char               role      = argv[1][0];
    const char*        peer_ip   = argv[2];
    int                my_port   = (role == 'A') ? PORT_A : PORT_B;
    int                peer_port = (role == 'A') ? PORT_B : PORT_A;
    int                sock;
    struct sockaddr_in addr, peer_addr;
    char               buffer[BUF_SIZE];
    socklen_t          addrlen = sizeof(peer_addr);

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[pingpong] socket failed");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(my_port);

    if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("[pingpong] bind failed");
        close(sock);
        return 1;
    }
    printf("[pingpong %c] bound to port %d\n", role, my_port);

    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port   = htons(peer_port);
    inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr);

    int counter = 0;

    if (role == 'A') {
        // A starts by sending PING:0
        int len = snprintf(buffer, BUF_SIZE, "PING:%d", counter);
        sendto(sock, buffer, len, 0, (struct sockaddr*) &peer_addr, addrlen);
        printf("[pingpong A] sent '%s'\n", buffer);
    }

    while (1) {
        ssize_t rlen = recvfrom(sock, buffer, BUF_SIZE - 1, 0, NULL, NULL);
        if (rlen < 0) {
            perror("[pingpong] recvfrom failed");
            break;
        }
        buffer[rlen] = '\0';
        printf("[pingpong %c] received '%s'\n", role, buffer);

        if (strncmp(buffer, "STOP", 4) == 0) {
            // Peer asked to stop
            printf("[pingpong %c] received STOP, exiting\n", role);
            break;
        }

        // Fragile parsing: assume format "PING:%d" or "PONG:%d"
        int parsed = 0, val = 0;
        if (sscanf(buffer, "PING:%d", &val) == 1) {
            // Received PING, reply with PONG:val+1
            counter = val + 1;
            int len = snprintf(buffer, BUF_SIZE, "PONG:%d", counter);
            sendto(sock, buffer, len, 0, (struct sockaddr*) &peer_addr, addrlen);
            printf("[pingpong %c] sent '%s'\n", role, buffer);
        } else if (sscanf(buffer, "PONG:%d", &val) == 1) {
            // Received PONG, reply with PING:val+1
            counter = val + 1;
            int len = snprintf(buffer, BUF_SIZE, "PING:%d", counter);
            sendto(sock, buffer, len, 0, (struct sockaddr*) &peer_addr, addrlen);
            printf("[pingpong %c] sent '%s'\n", role, buffer);
        } else {
            printf("[pingpong %c] unknown message format: %s\n", role, buffer);
        }
    }

    close(sock);
    return 0;
}
