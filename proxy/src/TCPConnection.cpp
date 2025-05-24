#include "TCPConnection.hpp"
#include <stdexcept>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

TCPConnection::TCPConnection(const std::string& name,
                             const utils::EntityConfig& e1,
                             const utils::EntityConfig& e2)
    : Connection(name, e1, e2) {}

void TCPConnection::init() {
    socket1 = socket(AF_INET, SOCK_STREAM, 0);
    socket2 = socket(AF_INET, SOCK_STREAM, 0);

    if (socket1 < 0 || socket2 < 0) {
        throw std::runtime_error("[TCPConnection] [" + this->name + "] Failed to create TCP sockets");
    }
}

void TCPConnection::run() {
    // Placeholder - logică viitoare
}

// void TCPConnection::close() {
//     if (socket1 != -1) close(socket1);
//     if (socket2 != -1) close(socket2);
// }

void TCPConnection::log(const std::string& msg) {
    printf("[TCPConnection][%s] %s\n", name.c_str(), msg.c_str());
}
