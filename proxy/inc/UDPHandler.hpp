#pragma once

#include <vector>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include "UDPConnection.hpp"
#include "ConfigurationManager.hpp"

class UDPHandler {
public:
    using Endpoint = std::pair<std::string, int>;

    struct ConnectionKey {
        Endpoint from;
        Endpoint to;

        bool operator<(const ConnectionKey& other) const {
            return std::tie(from, to) < std::tie(other.from, other.to);
        }
    };

    explicit UDPHandler(const std::vector<utils::EntityConfig>& entities);

    void buildGraphOnly();
    void printGraph() const;
    void buildConnections();
    void startConnectionsThreads();
    void printConnectionGraph() const;

private:
    std::vector<utils::EntityConfig> entities_;
    std::map<Endpoint, std::vector<Endpoint>> graph_;
    std::map<Endpoint, int> socket_map_;
    std::map<ConnectionKey, std::unique_ptr<UDPConnection>> connections_;
    std::vector<int> thread_ids_;
};
