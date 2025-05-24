#pragma once
#include <string>
#include "ConfigurationManager.hpp"

class Connection {
public:
    virtual ~Connection() = default;

    // Unique ID for connection
    std::string name;

    utils::EntityConfig endpoint1;
    utils::EntityConfig endpoint2;

    int socket1 = -1;
    int socket2 = -1;

protected:
    Connection(const std::string& name,
               const utils::EntityConfig& e1,
               const utils::EntityConfig& e2)
        : name(name), endpoint1(e1), endpoint2(e2) {}

public:
    virtual void init() = 0;
    virtual void run() = 0;
    //virtual void close() = 0;
    virtual void log(const std::string& msg) = 0;
};
