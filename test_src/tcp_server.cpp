#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_fd, 5);
    std::cout << "[SERVER] [TCP] Listening on port " << port << "...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        // Extract client info
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        std::cout << "[SERVER] Accepted connection from "
                  << client_ip << ":" << client_port << "\n";

        // Handle client in this thread (simple example)
        char buffer[1024];
        int counter = 1;
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int valread = read(client_fd, buffer, sizeof(buffer));
            if (valread <= 0) break;
            std::cout << "[SERVER] Received (" << counter++ << "): " << buffer << std::endl;
        }

        close(client_fd);
        std::cout << "[SERVER] Closed connection to " << client_ip << ":" << client_port << "\n";
    }

    close(server_fd);
    return 0;
}
