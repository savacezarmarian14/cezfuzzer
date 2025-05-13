#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <pthread.h>
#include <nlohmann/json.hpp>
#include "UDPHandler.hpp"
#include "TCPHandler.hpp"
#include <unistd.h>

using json = nlohmann::json;

// Struct for storing a destination endpoint
struct Endpoint {
    std::string ip;
    int port;
};

// Struct for each entity
struct EntityInfo {
    std::string role;
    std::string ip;
    int port;
    std::vector<Endpoint> destinations;
};

// Global containers for entities
std::map<std::string, EntityInfo> udp_entities;
std::map<std::string, EntityInfo> tcp_entities;

// Load entities_config.json into memory
bool load_entities_config(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Failed to open entities_config.json" << std::endl;
        return false;
    }

    json j;
    in >> j;

    if (j.contains("udp")) {
        for (const auto& [name, val] : j["udp"].items()) {
            EntityInfo info;
            info.role = val["role"];
            info.ip = val["ip"];
            info.port = val["port"];
            for (const auto& dest : val["destinations"]) {
                info.destinations.push_back({dest["ip"], dest["port"]});
            }
            udp_entities[name] = info;
        }
    }

    if (j.contains("tcp")) {
        for (const auto& [name, val] : j["tcp"].items()) {
            EntityInfo info;
            info.role = val["role"];
            info.ip = val["ip"];
            info.port = val["port"];
            for (const auto& dest : val["destinations"]) {
                info.destinations.push_back({dest["ip"], dest["port"]});
            }
            tcp_entities[name] = info;
        }
    }

    return true;
}

// Thread launchers
void* start_handle_udp_communication(void* arg) {
    // UDPHandler handler;
    // handler.run();
    sleep(100);
    return nullptr;
}

void* start_handle_tcp_connections(void* arg) {
    // TCPHandler handler;
    // handler.run();
    sleep(100);
    return nullptr;
}

int main() {
    if (!load_entities_config("entities_config.json")) {
        return 1;
    }

    pthread_t udp_thread, tcp_thread;

    if (!udp_entities.empty()) {
        if (pthread_create(&udp_thread, nullptr, start_handle_udp_communication, nullptr) != 0) {
            std::cerr << "Failed to create UDP thread\n";
            return 1;
        }
    }

    if (!tcp_entities.empty()) {
        if (pthread_create(&tcp_thread, nullptr, start_handle_tcp_connections, nullptr) != 0) {
            std::cerr << "Failed to create TCP thread\n";
            return 1;
        }
    }

    if (!udp_entities.empty()) {
        pthread_join(udp_thread, nullptr);
    }

    if (!tcp_entities.empty()) {
        pthread_join(tcp_thread, nullptr);
    }

    return 0;
}
