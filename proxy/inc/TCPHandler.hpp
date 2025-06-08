#ifndef TCP_HANDLER_HPP
#define TCP_HANDLER_HPP

#include <vector>
#include <string>
#include <pthread.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>

#include "TCPConnection.hpp"
#include "ConfigurationManager.hpp"
#include "Fuzzer.hpp"

class TCPHandler {
  public:
    TCPHandler(); // Default
    TCPHandler(const std::vector<utils::EntityConfig>&   tcp_entities,
               const std::vector<utils::TCPRedirection>& tcp_redirections);
    ~TCPHandler();

    void         addChannelPair(const TCP_ChannelPair& pair);
    void         printStatus() const;
    static void* _listen_thread(void* args);

  private:
    std::vector<utils::EntityConfig> _entities;
    std::vector<TCP_ChannelPair>     _channelPairs;
    std::vector<int>                 _listenSockets;
};

#endif // TCP_HANDLER_HPP
