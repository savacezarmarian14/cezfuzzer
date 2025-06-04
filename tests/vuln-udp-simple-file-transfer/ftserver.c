// file_server.c
//
// A simple UDP “file server” that listens on port 8000.
// On receiving "GET:<filename>", it attempts to open the file,
// fragments it into 512‐byte chunks, and sends each as
// "DATA:<seq_no>:<chunk_len>:<chunk_bytes>". Waits for "ACK:<seq_no>"
// before sending next chunk. After all chunks, sends "END_OF_FILE".
// Parsing is fragile: oversized GET or file not found cause odd behavior.

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 8000
#define BUF_SIZE    65536
#define CHUNK_SIZE  512

int main()
{
    int                sock;
    struct sockaddr_in srv_addr, cli_addr;
    char               buffer[BUF_SIZE];
    socklen_t          cli_len = sizeof(cli_addr);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[file_server] socket failed");
        return 1;
    }

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port        = htons(SERVER_PORT);

    if (bind(sock, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0) {
        perror("[file_server] bind failed");
        close(sock);
        return 1;
    }

    printf("[file_server] listening on port %d\n", SERVER_PORT);

    while (1) {
        ssize_t len = recvfrom(sock, buffer, BUF_SIZE - 1, 0, (struct sockaddr*) &cli_addr, &cli_len);
        if (len < 0) {
            perror("[file_server] recvfrom failed");
            continue;
        }
        buffer[len] = '\0';

        // Expect "GET:<filename>"
        char filename[256] = {0};
        if (sscanf(buffer, "GET:%255s", filename) != 1) {
            printf("[file_server] malformed GET: %.*s\n", (int) len, buffer);
            continue;
        }
        printf("[file_server] received GET for '%s'\n", filename);

        FILE* fp = fopen(filename, "rb");
        if (!fp) {
            // Fragile error reply; no real protocol for errors
            const char* err = "ERROR:FILE_NOT_FOUND";
            sendto(sock, err, strlen(err), 0, (struct sockaddr*) &cli_addr, cli_len);
            continue;
        }

        // Read file chunk by chunk
        int    seq_no = 0;
        size_t bytes_read;
        char   chunk_buf[CHUNK_SIZE];
        while ((bytes_read = fread(chunk_buf, 1, CHUNK_SIZE, fp)) > 0) {
            // Build "DATA:<seq_no>:<len>:<payload>"
            char header[64];
            int  hlen = snprintf(header, sizeof(header), "DATA:%d:%zu:", seq_no, bytes_read);
            char sendbuf[BUF_SIZE];
            memcpy(sendbuf, header, hlen);
            memcpy(sendbuf + hlen, chunk_buf, bytes_read);

            // Send DATA packet
            sendto(sock, sendbuf, hlen + bytes_read, 0, (struct sockaddr*) &cli_addr, cli_len);
            printf("[file_server] sent DATA seq %d (len %zu)\n", seq_no, bytes_read);

            // Wait for "ACK:<seq_no>"
            ssize_t rlen = recvfrom(sock, buffer, BUF_SIZE - 1, 0, (struct sockaddr*) &cli_addr, &cli_len);
            if (rlen < 0) {
                perror("[file_server] recvfrom ACK failed");
                break;
            }
            buffer[rlen] = '\0';
            int ack_no;
            if (sscanf(buffer, "ACK:%d", &ack_no) != 1 || ack_no != seq_no) {
                printf("[file_server] unexpected ACK: %s\n", buffer);
                break;
            }
            seq_no++;
        }

        fclose(fp);

        // Send "END_OF_FILE"
        const char* eof = "END_OF_FILE";
        sendto(sock, eof, strlen(eof), 0, (struct sockaddr*) &cli_addr, cli_len);
        printf("[file_server] sent END_OF_FILE\n");

        // Wait for "BYE"
        ssize_t rlen = recvfrom(sock, buffer, BUF_SIZE - 1, 0, (struct sockaddr*) &cli_addr, &cli_len);
        if (rlen > 0) {
            buffer[rlen] = '\0';
            if (strncmp(buffer, "BYE", 3) == 0) {
                printf("[file_server] received BYE, done with this client\n");
            }
        }
    }

    close(sock);
    return 0;
}
