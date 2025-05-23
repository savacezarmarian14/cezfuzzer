#include "UDPConnection.hpp"
#include <stdexcept>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

UDPConnection::UDPConnection(const std::string& name,
                             const utils::EntityConfig& e1,
                             const utils::EntityConfig& e2)
    : Connection(name, e1, e2) {}

void UDPConnection::init() {
    socket1 = socket(AF_INET, SOCK_DGRAM, 0);
    socket2 = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket1 < 0 || socket2 < 0) {
        throw std::runtime_error("Failed to create UDP sockets");
    }
}

void UDPConnection::run() {
    // Placeholder - logică viitoare
}

// void UDPConnection::close() {
//     if (socket1 != -1) close(socket1);
//     if (socket2 != -1) close(socket2);
// }

void UDPConnection::log(const std::string& msg) {
    printf("[UDPConnection][%s] %s\n", name.c_str(), msg.c_str());
}
