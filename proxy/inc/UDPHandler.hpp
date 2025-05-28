#pragma once

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include "Fuzzer.hpp"
#include "ConfigurationManager.hpp"

class UDPHandler {
  public:
    using Endpoint = std::pair<std::string, int>;

    explicit UDPHandler(const std::vector<utils::EntityConfig>& entities);

    void buildFromConnections(const std::vector<utils::Connection>& connections);
    void startRecvThreads();

    static UDPHandler*               instance_;
    static UDPHandler*               getInstance();
    std::map<int, utils::Connection> sock_to_connection_; // sockfd -> connection
    FuzzerCore                       fuzzer;

  private:
    std::vector<utils::EntityConfig> entities_;
    std::vector<int>                 recv_sockets_;
    std::vector<pthread_t>           threads_;
};
