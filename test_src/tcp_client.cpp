#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>\n";
        return 1;
    }

    const char* server_ip = argv[1];
    int port = std::stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    int counter = 1;
    while (true) {
        std::string msg = "[CLIENT] [TCP] Message no. " + std::to_string(counter++);
        send(sock, msg.c_str(), msg.size(), 0);
        sleep(1);
    }

    close(sock);
    return 0;
}
