#include "UDPHandler.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <error.h>
#include <string.h>


UDPHandler::UDPHandler(const std::vector<utils::EntityConfig>& entities)
    : entities_(entities) {}

void UDPHandler::buildGraphOnly() {
    for (const auto& entity : entities_) {
        Endpoint from = {entity.ip, entity.port};

        for (const auto& dest : entity.destinations) {
            Endpoint to = {dest.ip, dest.port};
            graph_[from].push_back(to);
        }
    }
}

void UDPHandler::printGraph() const {
    std::cout << "[UDPHandler] Logic Graph:\n";
    for (const auto& [from, targets] : graph_) {
        for (const auto& to : targets) {
            std::cout << "  " << from.first << ":" << from.second
                      << " --> " << to.first << ":" << to.second << "\n";
        }
    }
}

void UDPHandler::buildConnections() {
    for (const auto& [from, targets] : graph_) {

        // Creează socket dacă nu există
        if (socket_map_.find(from) == socket_map_.end()) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) {
                perror("socket");
                continue;
            }

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(from.second);
            addr.sin_addr.s_addr = inet_addr(from.first.c_str());

            socket_map_[from] = sock;
        }

        for (const auto& to : targets) {
            if (socket_map_.find(to) == socket_map_.end()) {
                int sock = socket(AF_INET, SOCK_DGRAM, 0);
                if (sock < 0) {
                    perror("socket");
                    continue;
                }

                socket_map_[to] = sock;
            }

            ConnectionKey key{from, to};
            utils::EntityConfig dummy_src{.ip = from.first, .port = from.second};
            utils::EntityConfig dummy_dst{.ip = to.first, .port = to.second};

            auto conn = std::make_unique<UDPConnection>(
                from.first + ":" + std::to_string(from.second) +
                "_to_" +
                to.first + ":" + std::to_string(to.second),
                dummy_src,
                dummy_dst
            );

            conn->socket1 = socket_map_[from];
            conn->socket2 = socket_map_[to];

            connections_[key] = std::move(conn);
        }
    }
}

void UDPHandler::printConnectionGraph() const 
{
    std::cout << "[UDPHandler] Connection Graph:\n";
    for (const auto& [key, conn] : connections_) {
        std::cout << "  " << key.from.first << ":" << key.from.second;

        if (auto it = socket_map_.find(key.from); it != socket_map_.end())
            std::cout << " [sock: " << it->second << "]";
        else
            std::cout << " [sock: not found]";

        std::cout << " --> " << key.to.first << ":" << key.to.second;

        if (auto it = socket_map_.find(key.to); it != socket_map_.end())
            std::cout << " [sock: " << it->second << "]";
        else
            std::cout << " [sock: not found]";

        std::cout << "\n";
    }
}

void* socketRecvThread(void* arg)
{
    int sock = *static_cast<int*>(arg);
    delete static_cast<int*>(arg); // release dynamic memory

    printf("[DEBUG] socketRecvThread started on socket %d\n", sock);

    char buffer[4096];
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    while (1) {
        ssize_t len = recvfrom(sock, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&src_addr, &addrlen);
        if (len < 0) {
            perror("[ERROR] recvfrom failed");
            continue;
        }

        printf("[DEBUG] Received %zd bytes on socket %d\n", len, sock);

        // TODO: enqueue to fuzz/send logic
    }

    return NULL;
}

void UDPHandler::startConnectionsThreads()
{
    for (const auto& [endpoint, sock] : socket_map_) {
        int* socket_ptr = new int(sock); // dynamic allocation for pthread argument

        pthread_t thread;
        int ret = pthread_create(&thread, nullptr, socketRecvThread, socket_ptr);
        if (ret != 0) {
            printf("[ERROR] Failed to create thread for socket %d: %s\n", sock, strerror(ret));
            delete socket_ptr;
            continue;
        }

        printf("[DEBUG] Started thread for socket %d\n", sock);

        thread_ids_.push_back((int)(uintptr_t)thread); // cast to int explicitly
    }
}