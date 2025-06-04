// vuln-server.c
// UDP server vulnerable to a stack overflow. Receives messages into a heap buffer (60 000 bytes),
// and after receiving the 10th message copies the heap buffer into a 64-byte stack buffer without bounds checking.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT        9999  // UDP port to listen on
#define HEAP_BUFSZ  60000 // size of the heap buffer for incoming data
#define STACK_BUFSZ 64    // size of the vulnerable stack buffer

int main(int argc, char* argv[])
{
    int                sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t          addr_len      = sizeof(client_addr);
    char*              heap_buffer   = NULL; // pointer to data on the heap
    int                message_count = 0;

    // Allocate a buffer of HEAP_BUFSZ + 1 bytes on the heap, to store incoming UDP messages.
    heap_buffer = malloc((HEAP_BUFSZ + 1) * sizeof(char));
    if (!heap_buffer) {
        fprintf(stderr, "malloc() failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // 1. Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        free(heap_buffer);
        exit(EXIT_FAILURE);
    }

    // 2. Bind the socket to INADDR_ANY:PORT
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        free(heap_buffer);
        exit(EXIT_FAILURE);
    }

    printf("vuln-server listening on UDP port %d…\n", PORT);

    while (1) {
        // 3. Receive data from a client into the heap buffer
        memset(heap_buffer, 0, HEAP_BUFSZ + 1);
        ssize_t len = recvfrom(sockfd, heap_buffer,
                               HEAP_BUFSZ, // allow up to 60000 bytes
                               0, (struct sockaddr*) &client_addr, &addr_len);

        if (len < 0) {
            perror("recvfrom");
            continue;
        }

        // Ensure null termination
        if (len >= HEAP_BUFSZ) {
            len = HEAP_BUFSZ - 1;
        }
        heap_buffer[len] = '\0';

        message_count++;
        printf("Message %d received from %s:%d – \"%s\"\n", message_count, inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port), heap_buffer);

        // After the 10th message, deliberately trigger a stack overflow
        if (message_count == 10) {
            printf(">>> Reached message #10. Triggering stack overflow…\n");

            // Declare a vulnerable buffer on the stack
            char stack_buffer[STACK_BUFSZ];
            memset(stack_buffer, 0, STACK_BUFSZ);

            // Copy the entire heap buffer (up to 60000 bytes) into the small stack buffer
            // using strcpy(), which does not check the destination size. This overruns
            // the 64-byte buffer and corrupts the stack (return addresses, saved registers, etc.).
            strcpy(stack_buffer, heap_buffer);

            // Print the first few bytes immediately after the stack buffer to show corruption
            printf("First 8 bytes after stack_buffer (after overflow):\n");
            unsigned char* ptr = (unsigned char*) stack_buffer;
            // stack_buffer occupies bytes [0 .. 63], so read starting at offset 64
            for (int i = STACK_BUFSZ; i < STACK_BUFSZ + 8; i++) {
                printf(" %02X", ptr[i]);
            }
            printf("\n");

            printf("After overflow, the server will likely crash when returning from this block.\n");
            // Once this block ends, the corrupted return address/saved EIP/RIP will cause a crash.
        }

        // Send a simple acknowledgment back to the client
        const char* ack = "ACK";
        sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr*) &client_addr, addr_len);
    }

    // Cleanup (never reached in this infinite loop, but good practice)
    close(sockfd);
    free(heap_buffer);
    return 0;
}
