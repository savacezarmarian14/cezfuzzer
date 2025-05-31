#ifndef UDP_CONNECTION_HPP
#define UDP_CONNECTION_HPP

#include <string>
#include <queue>
#include <pthread.h>
#include "ConfigurationManager.hpp" // include struct Connection

/**
 * @brief Represents a bidirectional UDP communication channel between two entities.
 */
class UDPConnection {
  public:
    // Constructor manual
    UDPConnection(const std::string& entityA_ip, int entityA_port, int port_entityA_recv, int port_entityA_send,
                  const std::string& entityB_ip, int entityB_port, int port_entityB_recv, int port_entityB_send)
        : entityA_ip_(entityA_ip), entityA_port_(entityA_port), port_entityA_recv_(port_entityA_recv),
          port_entityA_send_(port_entityA_send), entityB_ip_(entityB_ip), entityB_port_(entityB_port),
          port_entityB_recv_(port_entityB_recv), port_entityB_send_(port_entityB_send) {
        pthread_mutex_init(&dynamic_ports_mutex_, nullptr);
    }

    // Constructor din struct Connection
    UDPConnection(const utils::Connection& conn)
        : entityA_ip_(conn.entityA_ip), entityA_port_(conn.entityA_port),
          port_entityA_recv_(conn.entityA_proxy_port_recv), port_entityA_send_(conn.entityA_proxy_port_send),
          entityB_ip_(conn.entityB_ip), entityB_port_(conn.entityB_port),
          port_entityB_recv_(conn.entityB_proxy_port_recv), port_entityB_send_(conn.entityB_proxy_port_send) {
        pthread_mutex_init(&dynamic_ports_mutex_, nullptr);
    }

    const std::string& getEntityAIP() const { return entityA_ip_; }
    int                getEntityAPort() const { return entityA_port_; }
    int                getEntityARecvPort() const { return port_entityA_recv_; }
    int                getEntityASendPort() const { return port_entityA_send_; }

    const std::string& getEntityBIP() const { return entityB_ip_; }
    int                getEntityBPort() const { return entityB_port_; }
    int                getEntityBRecvPort() const { return port_entityB_recv_; }
    int                getEntityBSendPort() const { return port_entityB_send_; }

    int  getSendSockToEntityA() const { return send_sock_to_entityA_; }
    void setSendSockToEntityA(int sock) { send_sock_to_entityA_ = sock; }

    int  getSendSockToEntityB() const { return send_sock_to_entityB_; }
    void setSendSockToEntityB(int sock) { send_sock_to_entityB_ = sock; }

    int  getRecvSockFromEntityA() const { return recv_sock_from_entityA_; }
    void setRecvSockFromEntityA(int sock) { recv_sock_from_entityA_ = sock; }

    int  getRecvSockFromEntityB() const { return recv_sock_from_entityB_; }
    void setRecvSockFromEntityB(int sock) { recv_sock_from_entityB_ = sock; }

    void pushDynamicPort(int port) {
        pthread_mutex_lock(&dynamic_ports_mutex_);
        dynamic_ports_.push(port);
        pthread_mutex_unlock(&dynamic_ports_mutex_);
    }

    bool popDynamicPort(int& port) {
        pthread_mutex_lock(&dynamic_ports_mutex_);
        if (dynamic_ports_.empty()) {
            pthread_mutex_unlock(&dynamic_ports_mutex_);
            return false;
        }
        port = dynamic_ports_.front();
        dynamic_ports_.pop();
        pthread_mutex_unlock(&dynamic_ports_mutex_);
        return true;
    }

  private:
    std::string entityA_ip_;
    int         entityA_port_;
    int         port_entityA_recv_;
    int         port_entityA_send_;

    std::string entityB_ip_;
    int         entityB_port_;
    int         port_entityB_recv_;
    int         port_entityB_send_;

    int send_sock_to_entityA_   = -1;
    int send_sock_to_entityB_   = -1;
    int recv_sock_from_entityA_ = -1;
    int recv_sock_from_entityB_ = -1;

    std::queue<int> dynamic_ports_;
    pthread_mutex_t dynamic_ports_mutex_;
};

#endif // UDP_CONNECTION_HPP
