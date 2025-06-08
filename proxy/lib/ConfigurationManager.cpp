#include "ConfigurationManager.hpp"
#include <iostream>
#include <string.h>

namespace utils {

ConfigurationManager::ConfigurationManager(const std::string& config_path) : path_(config_path) {}

bool ConfigurationManager::parse()
{
    try {
        YAML::Node config = YAML::LoadFile(path_);

        // General
        if (config["general"]) {
            general_.log_level = config["general"]["log_level"].as<std::string>();
            general_.log_dir   = config["general"]["log_dir"].as<std::string>();
        }

        // Network
        if (config["network"]) {
            network_.docker_network_name = config["network"]["docker_network_name"].as<std::string>();
            network_.subnet              = config["network"]["subnet"].as<std::string>();
            network_.gateway             = config["network"]["gateway"].as<std::string>();
        }

        // Entities
        if (config["entities"]) {
            for (const auto& it : config["entities"]) {
                EntityConfig entity;
                entity.name            = it.first.as<std::string>();
                const YAML::Node& data = it.second;

                entity.role        = data["role"].as<std::string>();
                entity.protocol    = data["protocol"] ? data["protocol"].as<std::string>() : "";
                entity.ip          = data["ip"].as<std::string>();
                entity.port        = data["port"].as<int>();
                entity.fuzzed      = data["fuzzed"].as<bool>();
                entity.binary_path = data["binary_path"].as<std::string>();
                entity.exec_with   = data["exec_with"].as<std::string>();

                if (data["args"]) {
                    for (const auto& arg : data["args"]) {
                        entity.args.push_back(arg.as<std::string>());
                    }
                }

                if (data["destinations"]) {
                    for (const auto& dst : data["destinations"]) {
                        Destination d;
                        d.ip   = dst["ip"].as<std::string>();
                        d.port = dst["port"].as<int>();
                        entity.destinations.push_back(d);
                    }
                }

                if (data["connect_to"]) {
                    ConnectTo conn;
                    conn.ip           = data["connect_to"]["ip"].as<std::string>();
                    conn.port         = data["connect_to"]["port"].as<int>();
                    entity.connect_to = conn;
                }

                if (data["connections"]) {
                    for (const auto& cnode : data["connections"]) {
                        Connection c;
                        c.entityA_ip              = cnode["entityA_ip"].as<std::string>();
                        c.entityA_port            = cnode["entityA_port"].as<int>();
                        c.entityA_proxy_port_recv = cnode["entityA_proxy_port_recv"].as<int>();
                        c.entityA_proxy_port_send = cnode["entityA_proxy_port_send"].as<int>();
                        c.entityB_ip              = cnode["entityB_ip"].as<std::string>();
                        c.entityB_port            = cnode["entityB_port"].as<int>();
                        c.entityB_proxy_port_recv = cnode["entityB_proxy_port_recv"].as<int>();
                        c.entityB_proxy_port_send = cnode["entityB_proxy_port_send"].as<int>();
                        entity.connections.push_back(c);
                    }
                }

                if (data["tcp_redirections"]) {
                    for (const auto& cnode : data["tcp_redirections"]) {
                        TCPRedirection c;
                        c.server_ip   = cnode["server_ip"].as<std::string>();
                        c.server_port = cnode["server_port"].as<int>();
                        c.proxy_port  = cnode["proxy_port"].as<int>();
                        entity.tcp_redirections.push_back(c);
                    }
                }
                entities_.push_back(entity);
                if (entity.role == "fuzzer") {
                    fuzzer_ = entity;
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse config file: " << e.what() << "\n";
        return false;
    }

    return true;
}

GeneralConfig ConfigurationManager::getGeneralConfig() const
{
    return general_;
}

NetworkConfig ConfigurationManager::getNetworkConfig() const
{
    return network_;
}

std::vector<EntityConfig> ConfigurationManager::getEntities() const
{
    return entities_;
}

std::vector<EntityConfig> ConfigurationManager::getEntities(const char* IP)
{
    std::vector<EntityConfig> result;
    for (const auto& entity : entities_) {
        if (strcmp(entity.ip.c_str(), IP) == 0) {
            result.push_back(entity);
        }
    }
    return result;
}

EntityConfig ConfigurationManager::getFuzzer()
{
    EntityConfig result;
    for (const auto& entity : entities_) {
        if (entity.role == "fuzzer") {
            result = entity;
            break;
        }
    }
    return result;
}

} // namespace utils
