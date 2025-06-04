// client_vulnerable.c
//
// Vulnerable UDP client that sends DATA:<length>:<payload> messages to
// server on port 5000. It also unsafely parses the server’s ECHO:<length>:<payload>
// responses using a fixed-size buffer without validating the claimed length,
// resulting in a buffer-overflow vulnerability on the client side.

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

#define SERVER_PORT 5000
#define BUF_SIZE    65536 // Buffer for sending/receiving UDP packets
#define VULN_BUF    64    // Fixed-size buffer for vulnerable copy

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    const char*        server_ip = argv[1];
    int                sock;
    struct sockaddr_in server_addr;
    char               sendbuf[BUF_SIZE];
    char               recvbuf[BUF_SIZE];
    socklen_t          addrlen = sizeof(server_addr);

    // 1) Create UDP socket (no explicit bind; let kernel assign source port)
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[client] socket failed");
        return 1;
    }

    // 2) Configure server address (port 5000)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("[client] inet_pton failed");
        close(sock);
        return 1;
    }

    const char* payload = "TestPayload";
    int         counter = 0;

    while (1) {
        // Construct "DATA:<length>:<payload>" message
        int payload_len = strlen(payload);
        int datalen     = snprintf(sendbuf, BUF_SIZE, "DATA:%d:%s", payload_len, payload);
        if (datalen < 0) {
            perror("[client] snprintf failed");
            break;
        }

        // Send to server
        if (sendto(sock, sendbuf, datalen, 0, (struct sockaddr*) &server_addr, addrlen) < 0) {
            perror("[client] sendto failed");
            break;
        }
        printf("[client] sent '%.*s'\n", datalen, sendbuf);

        // Set receive timeout (500 ms)
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 500000; // 500 ms
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv)) < 0) {
            perror("[client] setsockopt failed");
            break;
        }

        // Receive ECHO response from server
        ssize_t rlen = recvfrom(sock, recvbuf, BUF_SIZE - 1, 0, NULL, NULL);
        if (rlen < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Timeout: no response
                printf("[client] no response (timeout)\n");
            } else {
                perror("[client] recvfrom failed");
                break;
            }
        } else {
            recvbuf[rlen] = '\0';
            printf("[client] got raw '%s'\n", recvbuf);

            // Vulnerable parsing of "ECHO:<length>:<payload>"
            if (strncmp(recvbuf, "ECHO:", 5) == 0) {
                int   claimed_len = 0;
                char* len_start   = recvbuf + 5;            // points to "<length>:<payload>"
                char* colon_ptr   = strchr(len_start, ':'); // find ':' after <length>
                if (colon_ptr) {
                    // Temporarily null-terminate to parse integer
                    *colon_ptr  = '\0';
                    claimed_len = atoi(len_start); // convert to integer
                    *colon_ptr  = ':';             // restore ':'
                } else {
                    // Malformed server response
                    continue;
                }

                // Fixed-size buffer for vulnerable copy
                char payload_buf[VULN_BUF];

                // Pointer to actual payload data
                char* payload_start = colon_ptr + 1;

                // *** VULNERABILITY: unbounded memcpy based on claimed_len ***
                // If claimed_len > VULN_BUF, this will overflow payload_buf.
                memcpy(payload_buf, payload_start, claimed_len);
                // *************************************************************

                // Null-terminate (if claimed_len < VULN_BUF), otherwise ensure last byte is '\0'
                if (claimed_len < VULN_BUF) {
                    payload_buf[claimed_len] = '\0';
                } else {
                    payload_buf[VULN_BUF - 1] = '\0';
                }

                printf("[client] parsed payload_buf='%s' (claimed_len=%d)\n", payload_buf, claimed_len);
            }
        }

        counter++;
        usleep(300000);
    }

    // Optionally send "BYE" before exit (not necessary for this example)
    // snprintf(sendbuf, BUF_SIZE, "BYE");
    // sendto(sock, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&server_addr, addrlen);

    close(sock);
    return 0;
}
