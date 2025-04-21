#include <iostream>
#include <thread>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

// Simplu fuzz: adaugă prefix
std::string fuzz(const std::string& input) {
    return "[FUZZED] " + input;
}

// Forward data între 2 socket-uri, cu opțional fuzz
void forward(int src_sock, int dst_sock, bool apply_fuzz, const std::string& label) {
    char buffer[4096];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recv(src_sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        std::string data(buffer, bytes);
        if (apply_fuzz) {
            data = fuzz(data);
        }

        send(dst_sock, data.c_str(), data.size(), 0);
        std::cout << "[" << label << "] Forwarded: " << data << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <listen_port> <server_ip>" << std::endl;
        return 1;
    }

    std::cout << "DEBUG MESSAGE 1" << std::endl;

    int listen_port = std::stoi(argv[1]);
    const char* server_ip = argv[2];
    int server_port = listen_port; // presupunem că serverul ascultă tot pe același port

    std::cout << "DEBUG MESSAGE 2" << std::endl;

    // Socket de ascultare
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return 1;
    }

    std::cout << "DEBUG MESSAGE 3" << std::endl;

    sockaddr_in listen_addr{};
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(listen_port);

    std::cout << "DEBUG MESSAGE 4" << std::endl;

    if (bind(listener, (sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listener, 10) < 0) {
        perror("listen");
        return 1;
    }
    std::cout << "DEBUG MESSAGE 5" << std::endl;

    std::cout << "[FUZZER] Listening on port " << listen_port << "...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        std::cout << "DEBUG MESSAGE 6" << std::endl;

        int client_sock = accept(listener, (sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        // Creează conexiune către serverul real
        int server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            perror("socket");
            close(client_sock);
            continue;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
        server_addr.sin_port = htons(server_port);

        if (connect(server_sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect to server");
            close(client_sock);
            close(server_sock);
            continue;
        }

        std::cout << "[+] New connection: client ↔ fuzzer ↔ server" << std::endl;

        // Threaduri bidirecționale
        std::thread t1(forward, client_sock, server_sock, true, "CLIENT→SERVER");
        std::thread t2(forward, server_sock, client_sock, false, "SERVER→CLIENT");

        t1.detach();
        t2.detach();
    }

    close(listener);
    return 0;
}
