#pragma once
#include "Connection.hpp"

class TCPConnection : public Connection {
  public:
    TCPConnection(const std::string& name, const utils::EntityConfig& e1, const utils::EntityConfig& e2);

    void init() override;
    void run() override;
    // void close() override;
    void log(const std::string& msg) override;
};
