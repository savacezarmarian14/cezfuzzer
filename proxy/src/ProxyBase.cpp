#include "ProxyBase.hpp"
#include <cstdio>

ProxyBase::ProxyBase(const char* path)
{
    cm = utils::ConfigurationManager(path);
     if (cm.parse() != true) {
        printf("[ERROR] utils::ConfigurationManager::parse()\n");
        exit(-1);
    }
    
    std::vector<utils::EntityConfig> entities = cm.getEntities();

    for (const auto& entity : entities) {
        if (entity.protocol == "tcp") {
            tcp_entities.push_back(entity);
        } else if (entity.protocol == "udp") {
            udp_entities.push_back(entity);
        }
    }

    for (const auto& entity : udp_entities) {
        printf("[ProxyBase] UDP entity: %s\n", entity.name.c_str());
    }
    for (const auto& entity : tcp_entities) {
        printf("[ProxyBase] TCP entity: %s\n", entity.name.c_str());
    }

    udp_handler_ = std::make_unique<UDPHandler>(udp_entities);
    udp_handler_->buildGraphOnly();
    udp_handler_->buildConnections();
    udp_handler_->printGraph();
    udp_handler_->printConnectionGraph();
    udp_handler_->startConnectionsThreads();
}
