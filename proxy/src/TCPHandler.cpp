#include "TCPHandler.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

struct ListenThreadArgs {
    int         port;
    std::string ip;
    int         proxy_port;
    int         listen_fd;
    TCPHandler* handler;
};

TCPHandler::TCPHandler() {}

TCPHandler::TCPHandler(const std::vector<utils::EntityConfig>&   tcp_entities,
                       const std::vector<utils::TCPRedirection>& tcp_redirections)
    : _entities(tcp_entities)
{

    for (const auto& redir : tcp_redirections) {
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            std::cerr << "[TCPHandler] Failed to create socket for proxy_port " << redir.proxy_port << "\n";
            continue;
        }

        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = inet_addr("0.0.0.0"); // Bind to all interfaces
        addr.sin_port        = htons(redir.proxy_port);

        if (bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
            std::cerr << "[TCPHandler] Failed to bind to proxy_port " << redir.proxy_port << "\n";
            close(listen_fd);
            continue;
        }

        if (listen(listen_fd, 10) < 0) {
            std::cerr << "[TCPHandler] Listen failed on port " << redir.proxy_port << "\n";
            close(listen_fd);
            continue;
        }

        _listenSockets.push_back(listen_fd);
        std::cout << "[TCPHandler] Listening on 0.0.0.0:" << redir.proxy_port << "\n";

        // Launch accept thread
        auto*     args = new ListenThreadArgs{redir.server_port, redir.server_ip, redir.proxy_port, listen_fd, this};
        pthread_t tid;
        if (pthread_create(&tid, nullptr, _listen_thread, args) != 0) {
            std::cerr << "[TCPHandler] Failed to start thread for port " << redir.proxy_port << "\n";
            close(listen_fd);
            delete args;
        } else {
            pthread_detach(tid);
        }
    }
}

TCPHandler::~TCPHandler() {}

void TCPHandler::addChannelPair(const TCP_ChannelPair& pair)
{
    _channelPairs.push_back(pair);
    std::cout << "[TCPHandler] Added channel pair.\n";
}

void TCPHandler::printStatus() const
{
    std::cout << "[TCPHandler] Total active pairs: " << _channelPairs.size() << "\n";
}

void* TCPHandler::_listen_thread(void* args)
{
    int   ret  = 0;
    auto* data = static_cast<ListenThreadArgs*>(args);
    std::cout << "[ListenThread] Accepting for " << data->ip << ":" << data->port << "\n";

    sockaddr_in client_addr{};
    socklen_t   client_len = sizeof(client_addr);

    while (true) {
        int _from_clinet_fd = accept(data->listen_fd, (struct sockaddr*) &client_addr, &client_len);
        if (_from_clinet_fd < 0) {
            std::cerr << "[ListenThread] Accept failed on port " << data->proxy_port << "\n";
            std::cerr << "[DEBUG] " << data->listen_fd << "\n";
            sleep(3);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        uint16_t client_port = ntohs(client_addr.sin_port);

        std::cout << "[ListenThread] New client connected → " << client_ip << ":" << client_port << "\n";
        TCP_Connection _form_clinet_connection(_from_clinet_fd, std::string(client_ip), client_port);

        int _to_server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_to_server_fd < 0) {
            std::cerr << "[TCPHandler] Failed to create socket for server " << data->ip << ":" << data->port << "\n";
            exit(-1);
        }

        sockaddr_in addr;
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = inet_addr(data->ip.c_str()); // Bind to all interfaces
        addr.sin_port        = htons(data->port);
        sleep(1);
        ret = connect(_to_server_fd, (struct sockaddr*) &addr, sizeof(addr));
        if (ret < 0) {
            std::cerr << "[TCPHandler] Failed to connect socket to server " << data->ip << ":" << data->port << "\n";
            exit(-1);
        }
        std::cout << "[TCPHandler] Socket connected to server " << data->ip << ":" << data->port << "\n";
        TCP_Connection  _to_server_connection(_to_server_fd, data->ip, data->port);
        TCP_ChannelPair _pair;
        _pair.setClientSide(_form_clinet_connection);
        _pair.setServerSide(_to_server_connection);
        _pair.startChannelThreads();
        data->handler->addChannelPair(_pair);
        std::cout << "[TCPHandler] Channel initialised\n";
    }

    delete data;
    return nullptr;
}
