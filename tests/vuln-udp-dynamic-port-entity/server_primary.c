// server_vulnerable.c
//
// Vulnerable UDP server that listens on port 5000. It parses incoming
// messages of the form "DATA:<length>:<payload>" but does not properly
// validate <length> before copying into a fixed-size buffer, resulting
// in a classic buffer-overflow vulnerability that fuzzers can trigger.

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 5000
#define BUF_SIZE    65536

int main()
{
    int                sock;
    struct sockaddr_in server_addr, client_addr;
    char               buffer[BUF_SIZE];
    socklen_t          addrlen = sizeof(client_addr);

    // 1) Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[server] socket failed");
        return 1;
    }

    // 2) Bind to port 5000 on all interfaces
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port        = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("[server] bind failed");
        close(sock);
        return 1;
    }

    printf("[server] listening on port %d...\n", SERVER_PORT);

    // 3) Main loop: receive from any client and process
    while (1) {
        ssize_t len = recvfrom(sock, buffer, BUF_SIZE - 1, 0, (struct sockaddr*) &client_addr, &addrlen);
        if (len < 0) {
            perror("[server] recvfrom failed");
            continue;
        }
        buffer[len] = '\0';

        // Check for "BYE" command to log and continue
        if (strncmp(buffer, "BYE", 3) == 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            printf("[server] received BYE from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
            continue;
        }

        // Vulnerable parsing: expect "DATA:<length>:<payload>"
        if (strncmp(buffer, "DATA:", 5) == 0) {
            int   claimed_len = 0;
            char* len_start   = buffer + 5;             // points to "<length>:<payload>"
            char* colon_ptr   = strchr(len_start, ':'); // find the ':' after <length>
            if (colon_ptr) {
                // Parse <length>
                *colon_ptr  = '\0';            // temporarily terminate length string
                claimed_len = atoi(len_start); // convert to integer
                *colon_ptr  = ':';             // restore ':'
            } else {
                // Malformed message: no second colon
                continue;
            }

            // Vulnerable buffer: fixed size 64 bytes
            char payload_buf[64];

            // Calculate pointer to actual payload data
            char* payload_start = colon_ptr + 1;

            // *** VULNERABILITY HERE ***
            // We blindly copy 'claimed_len' bytes from payload_start into payload_buf,
            // without checking if claimed_len < sizeof(payload_buf). A fuzzer can send
            // a huge claimed_len to overflow payload_buf and overwrite the stack.
            memcpy(payload_buf, payload_start, claimed_len);
            // ***************************

            // Null-terminate so we can print safely (but if overflow occurred,
            // behavior is undefined)
            if (claimed_len < (int) sizeof(payload_buf)) {
                payload_buf[claimed_len] = '\0';
            } else {
                payload_buf[sizeof(payload_buf) - 1] = '\0';
            }

            printf("[server] received DATA with claimed_len=%d, payload_buf='%s'\n", claimed_len, payload_buf);

            // Echo back: "ECHO:<length>:<payload_buf>"
            char reply[BUF_SIZE];
            int  rlen = snprintf(reply, BUF_SIZE, "ECHO:%d:%s", claimed_len, payload_buf);
            if (rlen < 0) {
                continue;
            }
            ssize_t sent = sendto(sock, reply, (size_t) rlen, 0, (struct sockaddr*) &client_addr, addrlen);
            if (sent < 0) {
                perror("[server] sendto failed");
            }
        } else {
            // Unrecognized message format; ignore
            continue;
        }
    }

    close(sock);
    return 0;
}
