#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>

void receive_loop(int sock, sockaddr_in& local_addr) {
    if (bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    char buffer[1024];
    sockaddr_in sender_addr{};
    socklen_t addr_len = sizeof(sender_addr);

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&sender_addr, &addr_len);
        if (n > 0) {
            std::cout << "Received: " << buffer << std::endl;
        }
    }
}

void send_loop(int sock, sockaddr_in& dest_addr, const std::string& label) {
    int counter = 1;
    while (true) {
        std::string msg = "[" + label + "] [UDP] Message no. " + std::to_string(counter++);
        sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&dest_addr, sizeof(dest_addr));
        sleep(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <local_port> <dest_ip> <dest_port>\n";
        return 1;
    }

    int local_port = std::stoi(argv[1]);
    const char* dest_ip = argv[2];
    int dest_port = std::stoi(argv[3]);

    std::string label = "ENTITY on port " + std::to_string(local_port);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in local_addr{}, dest_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(local_port);

    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr);
    dest_addr.sin_port = htons(dest_port);

    std::thread recv_thread(receive_loop, sock, std::ref(local_addr));
    std::thread send_thread(send_loop, sock, std::ref(dest_addr), label);

    recv_thread.join();
    send_thread.join();

    close(sock);
    return 0;
}
