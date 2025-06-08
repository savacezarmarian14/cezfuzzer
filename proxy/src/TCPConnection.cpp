#include "TCPConnection.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "Fuzzer.hpp"

// ========== TCP_Connection ==========

TCP_Connection::TCP_Connection() : socket_fd_(-1), ip_(""), port_(0) {}

TCP_Connection::TCP_Connection(int fd) : socket_fd_(fd), ip_(""), port_(0) {}

TCP_Connection::TCP_Connection(int fd, std::string ip, uint16_t port) : socket_fd_(fd), ip_(ip), port_(port) {}

TCP_Connection::~TCP_Connection() {}

int TCP_Connection::getFD() const
{
    return socket_fd_;
}

std::string TCP_Connection::getIP() const
{
    return ip_;
}

uint16_t TCP_Connection::getPort() const
{
    return port_;
}

void TCP_Connection::setFD(int fd)
{
    socket_fd_ = fd;
}

void TCP_Connection::setIP(const std::string& ip)
{
    ip_ = ip;
}

void TCP_Connection::setPort(uint16_t port)
{
    port_ = port;
}

void TCP_Connection::startConnectionThread(int forward_fd)
{
    int       ret = 0;
    pthread_t _thread;
    struct _targ {
        int recvfd;
        int sendfd;
    }* _thread_arg;
    _thread_arg         = (struct _targ*) malloc(sizeof(*_thread_arg));
    _thread_arg->recvfd = this->getFD();
    _thread_arg->sendfd = forward_fd;

    ret = pthread_create(&_thread, NULL, TCP_Connection::_connection_thread_loop, _thread_arg);
    if (ret < 0) {
        std::cerr << "[TCP_Connection] Couldn't start thread TCPConnection\n";
        free(_thread_arg);
        exit(-1);
    }
}

// Placeholder for future threaded handling of connection
void* TCP_Connection::_connection_thread_loop(void* args)
{
    FuzzerCore _fuzzer(FUZZSTYLE_RANDOMIZATION);
    struct _targ {
        int recvfd;
        int sendfd;
    }*      _thread_arg = (struct _targ*) args;
    int     recv_fd     = _thread_arg->recvfd;
    int     send_fd     = _thread_arg->sendfd;
    ssize_t ret         = 0;
    free(_thread_arg);

    char buffer[65536];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ret = recv(recv_fd, buffer, sizeof(buffer), 0);
        std::cout << "[TCPConenction] Received " << buffer << "\n";
        if (ret < 0) {
            std::cerr << "[ERROR] Error at receving message on socket " << recv_fd << "\n";
            exit(-1);
        }
        size_t   fuzzedSize = 0;
        uint8_t* fuzzedBuff =
            _fuzzer.postFuzzing(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(ret), fuzzedSize);

        std::cout << "[TCPConnection] Forwarding ..." << "\n";
        ret = send(send_fd, fuzzedBuff, fuzzedSize, 0);
        free(fuzzedBuff);
    }

    return nullptr;
}

// ========== TCP_ChannelPair ==========

TCP_ChannelPair::TCP_ChannelPair() {}

TCP_ChannelPair::~TCP_ChannelPair()
{
    // Destructor will rely on TCP_Connection closing its own socket
}

TCP_Connection& TCP_ChannelPair::getClientSide()
{
    return client_side_;
}

TCP_Connection& TCP_ChannelPair::getServerSide()
{
    return server_side_;
}

void TCP_ChannelPair::setClientSide(const TCP_Connection& conn)
{
    client_side_ = conn;
}

void TCP_ChannelPair::setServerSide(const TCP_Connection& conn)
{
    server_side_ = conn;
}

void TCP_ChannelPair::startChannelThreads()
{
    this->client_side_.startConnectionThread(this->server_side_.getFD());
    this->server_side_.startConnectionThread(this->client_side_.getFD());
}