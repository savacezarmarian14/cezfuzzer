#pragma once
#include "Connection.hpp"

class UDPConnection : public Connection {
public:
    UDPConnection(const std::string& name,
                  const utils::EntityConfig& e1,
                  const utils::EntityConfig& e2);

    void init() override;
    void run() override;
    //void close() override;
    void log(const std::string& msg) override;
};
