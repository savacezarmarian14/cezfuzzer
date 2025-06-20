#pragma once

#include "ConfigurationManager.hpp"
#include "UDPHandler.hpp"
#include "TCPHandler.hpp"
#include <vector>

class ProxyBase {
  public:
    explicit ProxyBase(const char* path);
    const std::vector<utils::EntityConfig>& getUdpEntities() const { return udp_entities; }
    const std::vector<utils::EntityConfig>& getTcpEntities() const { return tcp_entities; }

  private:
    utils::ConfigurationManager      cm;
    std::vector<utils::EntityConfig> udp_entities;
    std::vector<utils::EntityConfig> tcp_entities;

    std::unique_ptr<UDPHandler> udp_handler_;
    std::unique_ptr<TCPHandler> tcp_handler_;
};
