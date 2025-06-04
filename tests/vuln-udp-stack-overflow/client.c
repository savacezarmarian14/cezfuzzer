// client.c
// Client UDP simplu care trimite exact 10 mesaje mici către serverul UDP.
// Acest client NU declanșează overflow-ul – el doar transmite mesaje “sigure”.
// Overflow-ul va fi declanșat de server după ce a numărat 10 mesaje primite.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define NUM_MESSAGES   10
#define MESSAGE_MAXLEN 32 // trimitem mesaje scurte (<<64 bytes)

int main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-ip> <server-port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char*        server_ip   = argv[1];
    int                server_port = atoi(argv[2]);
    int                sockfd;
    struct sockaddr_in server_addr;

    // 1. Creează socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Configurează adresa serverului
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", server_ip);
        close(sockfd);
        return EXIT_FAILURE;
    }
    server_addr.sin_port = htons(server_port);

    // 3. Trimite 10 mesaje “sigure” către server
    for (int i = 1; i <= NUM_MESSAGES; i++) {
        char message[MESSAGE_MAXLEN];
        // Construit simplu, mesajul “Mesaj_i”
        snprintf(message, sizeof(message), "Mesaj_%d", i);

        ssize_t sent =
            sendto(sockfd, message, strlen(message), 0, (struct sockaddr*) &server_addr, sizeof(server_addr));

        if (sent < 0) {
            perror("sendto");
            close(sockfd);
            return EXIT_FAILURE;
        }

        printf("[%d/%d] Am trimis: \"%s\"\n", i, NUM_MESSAGES, message);

        // Pentru claritate, putem face o pauză mică
        usleep(100 * 1000); // 100 ms
    }

    printf("Client: Am trimis toate cele %d mesaje.\n", NUM_MESSAGES);

    close(sockfd);
    return EXIT_SUCCESS;
}
