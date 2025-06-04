// file_client.c
//
// A minimal UDP client that sends "GET:<filename>" to server port 8000,
// receives DATA blocks, sends "ACK:<seq_no>" for each, and on "END_OF_FILE"
// replies "BYE". Fragile parsing can break if server’s responses are mangled.

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 8000
#define BUF_SIZE    65536

int main(int argc, char** argv)
{
    if (argc != 3) {
        printf("Usage: %s <server_ip> <filename>\n", argv[0]);
        return 1;
    }

    const char*        server_ip = argv[1];
    const char*        filename  = argv[2];
    int                sock;
    struct sockaddr_in srv_addr;
    char               buffer[BUF_SIZE];
    socklen_t          addrlen = sizeof(srv_addr);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[file_client] socket failed");
        return 1;
    }

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port   = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &srv_addr.sin_addr);

    // Send "GET:<filename>"
    int slen = snprintf(buffer, BUF_SIZE, "GET:%s", filename);
    sendto(sock, buffer, slen, 0, (struct sockaddr*) &srv_addr, addrlen);
    printf("[file_client] sent GET:%s\n", filename);

    while (1) {
        ssize_t len = recvfrom(sock, buffer, BUF_SIZE - 1, 0, (struct sockaddr*) &srv_addr, &addrlen);
        if (len < 0) {
            perror("[file_client] recvfrom failed");
            break;
        }
        buffer[len] = '\0';

        if (strncmp(buffer, "DATA:", 5) == 0) {
            // Expect "DATA:<seq_no>:<len>:<payload>"
            int   seq_no      = -1;
            int   payload_len = 0;
            char* ptr         = buffer + 5;
            // Fragile sscanf: no check for out‐of‐bounds
            if (sscanf(ptr, "%d:%d%n", &seq_no, &payload_len, &payload_len) < 2) {
                printf("[file_client] malformed DATA: %s\n", buffer);
                break;
            }
            // payload begins after two colons (skip digits and colons)
            // Very fragile: assumes exactly two colons before payload
            char* data_ptr = strchr(ptr, ':');
            data_ptr       = strchr(data_ptr + 1, ':');
            data_ptr++;

            printf("[file_client] got DATA seq=%d, len=%d\n", seq_no, payload_len);

            // Immediately send ACK:<seq_no>
            int alen = snprintf(buffer, BUF_SIZE, "ACK:%d", seq_no);
            sendto(sock, buffer, alen, 0, (struct sockaddr*) &srv_addr, addrlen);
        } else if (strcmp(buffer, "END_OF_FILE") == 0) {
            printf("[file_client] received END_OF_FILE\n");
            // Send BYE
            strcpy(buffer, "BYE");
            sendto(sock, buffer, 3, 0, (struct sockaddr*) &srv_addr, addrlen);
            break;
        } else {
            printf("[file_client] unexpected msg: %s\n", buffer);
            break;
        }
    }

    close(sock);
    return 0;
}
