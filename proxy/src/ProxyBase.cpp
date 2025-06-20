#include "ProxyBase.hpp"
#include <cstdio>
#include <string.h>

ProxyBase::ProxyBase(const char* path)
{
    cm = utils::ConfigurationManager(path);
    if (!cm.parse()) {
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

    // Folosim direct fuzzer-ul din config
    utils::EntityConfig fuzzer  = cm.getFuzzer();
    char*               proxyIP = strdup(fuzzer.ip.c_str());
    if (udp_entities.size() > 0) {
        udp_handler_ = std::make_unique<UDPHandler>(udp_entities, proxyIP);
        udp_handler_->buildFromConnections(fuzzer.connections);
        udp_handler_->startRecvThreads();
        udp_handler_->startSendThreads();
    }

    if (tcp_entities.size() > 0) {
        tcp_handler_ = std::make_unique<TCPHandler>(tcp_entities, fuzzer.tcp_redirections);
    }
}
