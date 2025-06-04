#ifndef UDP_HANDLER_HPP
#define UDP_HANDLER_HPP

#include <vector>
#include <unordered_map>
#include <string>
#include <pthread.h>
#include <memory>

#include "UDPConnection.hpp"
#include "ConfigurationManager.hpp"
#include "Fuzzer.hpp"

#define MAX_UDP_PAYLOAD_SIZE 65500

class UDPHandler {
  public:
    static UDPHandler* getInstance();
    UDPHandler(const std::vector<utils::EntityConfig>& entities, char* ip);

    void buildFromConnections(std::vector<utils::Connection>& connections);
    void startRecvThreads();
    void startSendThreads();

    // Getteri
    const std::vector<int>&                        getRecvSockets() const;
    const std::vector<UDPConnection*>&             getConnections() const;
    const std::unordered_map<int, UDPConnection*>& getSocketToConnectionMap() const;

  private:
    static UDPHandler* instance_;

    std::vector<utils::EntityConfig> entities_;
    std::string                      proxyIP_;
    FuzzerCore                       fuzzer;

    std::vector<int> recv_sockets_;
    std::vector<int> send_sockets_;

    std::vector<pthread_t>                  threads_;
    std::vector<UDPConnection*>             connections_;
    std::unordered_map<int, UDPConnection*> sock_to_connection_;

    int  createAndBindSocket(int port, bool transparent);
    void setupUDPConnection(std::unique_ptr<UDPConnection> conn);

    friend void* socketRecvThread(void* arg);
    static void* recvThreadEntry(void* arg);
    static void* sendThreadEntry(void* arg);
};

#endif // UDP_HANDLER_HPP
