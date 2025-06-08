#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>
#include <queue>
#include <yaml-cpp/yaml.h>

namespace utils {

struct Destination {
    std::string ip;
    int         port;
};

struct ConnectTo {
    std::string ip;
    int         port;
};

struct Connection {
    // Entity A
    std::string entityA_ip;
    int         entityA_port;
    int         entityA_proxy_port_recv; // bound with IP_RECVORIGDSTADDR
    int         entityA_proxy_port_send; // bound with IP_TRANSPARENT
    int         recv_sock_from_entityA = -1;
    int         send_sock_to_entityA   = -1;

    // Entity B
    std::string entityB_ip;
    int         entityB_port;
    int         entityB_proxy_port_recv;
    int         entityB_proxy_port_send;
    int         recv_sock_from_entityB = -1;
    int         send_sock_to_entityB   = -1;
};

struct TCPRedirection {
    std::string server_ip;
    uint16_t    server_port;
    uint16_t    proxy_port;
};

struct EntityConfig {
    std::string              name;
    std::string              role;
    std::string              protocol;
    std::string              ip;
    int                      port;
    bool                     fuzzed;
    std::string              binary_path;
    std::string              exec_with;
    std::vector<std::string> args;

    // Optional
    std::vector<Destination>    destinations;
    std::optional<ConnectTo>    connect_to;
    std::vector<Connection>     connections;
    std::vector<TCPRedirection> tcp_redirections;
};

struct GeneralConfig {
    std::string log_level;
    std::string log_dir;
};

struct NetworkConfig {
    std::string docker_network_name;
    std::string subnet;
    std::string gateway;
};

class ConfigurationManager {
  public:
    ConfigurationManager(const std::string& config_path);
    ConfigurationManager() = default;

    bool parse();

    GeneralConfig             getGeneralConfig() const;
    NetworkConfig             getNetworkConfig() const;
    std::vector<EntityConfig> getEntities() const;
    std::vector<EntityConfig> getEntities(const char* IP);
    EntityConfig              getFuzzer();

  private:
    std::string                 path_;
    GeneralConfig               general_;
    NetworkConfig               network_;
    std::vector<EntityConfig>   entities_;
    std::optional<EntityConfig> fuzzer_;
};

} // namespace utils
