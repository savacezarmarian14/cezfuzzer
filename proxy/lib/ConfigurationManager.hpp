#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
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
    std::string src_ip;
    int         src_port;
    int         port_src_proxy;
    int         sock_send_to_src; // socket folosit pentru trimitere către src

    std::string dst_ip;
    int         dst_port;
    int         port_dst_proxy;
    int         sock_send_to_dst; // socket folosit pentru trimitere către dst
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
    std::vector<Destination> destinations;
    std::optional<ConnectTo> connect_to;
    std::vector<Connection>  connections;
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
