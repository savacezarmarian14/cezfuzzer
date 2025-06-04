#include "UDPHandler.hpp"
#include "UDPConnection.hpp"

#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

UDPHandler* UDPHandler::instance_ = nullptr;

UDPHandler::UDPHandler(const std::vector<utils::EntityConfig>& entities, char* ip)
    : entities_(entities), proxyIP_(ip), fuzzer(FUZZSTYLE_RANDOMIZATION)
{
    instance_ = this;
    std::cout << "[DEBUG] UDPHandler initialized with proxy IP: " << proxyIP_ << std::endl;
}

UDPHandler* UDPHandler::getInstance()
{
    return instance_;
}

int UDPHandler::createAndBindSocket(int port, bool useTransparent)
{
    std::cout << "[DEBUG] Creating UDP socket on port " << port << " with "
              << (useTransparent ? "IP_TRANSPARENT" : "IP_RECVORIGDSTADDR") << std::endl;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket");
        return -1;
    }

    int optval  = 1;
    int optname = useTransparent ? IP_TRANSPARENT : IP_RECVORIGDSTADDR;
    if (setsockopt(sock, SOL_IP, optname, &optval, sizeof(optval)) < 0) {
        perror("[ERROR] setsockopt");
        close(sock);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = inet_addr(proxyIP_.c_str());

    if (bind(sock, (sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("[ERROR] bind");
        close(sock);
        return -1;
    }

    std::cout << "[DEBUG] Socket bound successfully to " << proxyIP_ << ":" << port << std::endl;
    return sock;
}

void UDPHandler::buildFromConnections(std::vector<utils::Connection>& conns)
{
    std::cout << "[DEBUG] Starting to build connections..." << std::endl;

    for (const auto& conn_struct : conns) {
        std::unique_ptr<UDPConnection> conn = std::make_unique<UDPConnection>(conn_struct);

        int recvA = createAndBindSocket(conn->getEntityARecvPort(), false);
        int sendB = createAndBindSocket(conn->getEntityBSendPort(), true);
        int recvB = createAndBindSocket(conn->getEntityBRecvPort(), false);
        int sendA = createAndBindSocket(conn->getEntityASendPort(), true);

        if (recvA >= 0 && sendB >= 0 && recvB >= 0 && sendA >= 0) {
            conn->setRecvSockFromEntityA(recvA);
            conn->setSendSockToEntityB(sendB);
            conn->setRecvSockFromEntityB(recvB);
            conn->setSendSockToEntityA(sendA);

            sock_to_connection_[recvA] = conn.get();
            sock_to_connection_[recvB] = conn.get();

            recv_sockets_.push_back(recvA);
            recv_sockets_.push_back(recvB);

            sock_to_connection_[sendA] = conn.get();
            sock_to_connection_[sendB] = conn.get();

            send_sockets_.push_back(sendA);
            send_sockets_.push_back(sendB);

            connections_.push_back(conn.release());
            std::cout << "[INFO] UDPConnection fully initialized and sockets mapped." << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to bind all sockets for a connection." << std::endl;
        }
    }
}

void UDPHandler::startRecvThreads()
{
    std::cout << "[DEBUG] Launching receiver threads for each recv socket..." << std::endl;

    for (int sock : recv_sockets_) {
        if (sock_to_connection_.count(sock) == 0) {
            std::cerr << "[ERROR] No mapping found for socket FD " << sock << std::endl;
            continue;
        }

        UDPConnection* conn = sock_to_connection_[sock];
        pthread_t      tid;

        if (pthread_create(&tid, nullptr, &UDPHandler::recvThreadEntry,
                           new std::pair<int, UDPConnection*>(sock, conn)) != 0) {
            perror("[ERROR] pthread_create failed");
        } else {
            pthread_detach(tid);
            std::cout << "[DEBUG] Thread launched for socket FD: " << sock << std::endl;
        }
    }

    std::cout << "[INFO] All UDP recv threads launched." << std::endl;
}

void* UDPHandler::recvThreadEntry(void* arg)
{
    auto [recv_sock, conn] = *static_cast<std::pair<int, UDPConnection*>*>(arg);
    delete static_cast<std::pair<int, UDPConnection*>*>(arg);

    // 1) Începem prin a prelua referința la fuzzer
    UDPHandler* handler = UDPHandler::getInstance();
    FuzzerCore& fuzzer  = handler->fuzzer;

    char               buffer[4096] = {0};
    struct sockaddr_in src_addr {};
    socklen_t          addrlen = sizeof(src_addr);

    std::cout << "[TYPE] [RECV THREAD] Listening on FD " << recv_sock << std::endl;

    while (true) {
        ssize_t len = recvfrom(recv_sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&src_addr), &addrlen);
        if (len <= 0) {
            perror("[TYPE] [RECV THREAD] recvfrom failed");
            break;
        }

        char src_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, sizeof(src_ip));
        int src_port = ntohs(src_addr.sin_port);

        printf("[TYPE] [RECV THREAD] Received %zd bytes on socket %d from %s:%u\n", len, recv_sock, src_ip, src_port);

        std::string target_ip;
        int         target_port = -1;
        int         send_sock   = -1;

        // Determinăm direcția (A→B sau B→A) și setăm target_ip/target_port/send_sock
        if (recv_sock == conn->getRecvSockFromEntityA()) {
            send_sock   = conn->getSendSockToEntityB();
            target_ip   = conn->getEntityBIP();
            target_port = conn->getEntityBPort();
            if (target_port == -1) {
                conn->popDynamicPort(target_port);
            }
            if (conn->getEntityAPort() == -1) {
                conn->pushDynamicPort(src_port);
                std::cout << "[TYPE] [RECV THREAD] Stored dynamic port from A: " << src_port << std::endl;
            }
        } else if (recv_sock == conn->getRecvSockFromEntityB()) {
            send_sock   = conn->getSendSockToEntityA();
            target_ip   = conn->getEntityAIP();
            target_port = conn->getEntityAPort();
            if (target_port == -1) {
                conn->popDynamicPort(target_port);
            }
            if (conn->getEntityBPort() == -1) {
                conn->pushDynamicPort(src_port);
                std::cout << "[TYPE] [RECV THREAD] Stored dynamic port from B: " << src_port << std::endl;
            }
        } else {
            std::cerr << "[TYPE] [RECV THREAD] recv_sock not recognized in connection." << std::endl;
            continue;
        }

        std::cout << "[TYPE] [RECV THREAD] Forwarding " << len << " bytes from " << src_ip << ":" << src_port << " to "
                  << target_ip << ":" << target_port << std::endl;

        // === AICI înserezi apelul la fuzzer ===
        // 2) Copiem datele primite într-un buffer temporar (uint8_t*),
        //    apoi apelăm, de exemplu, postFuzzing:
        size_t   fuzzedSize = 0;
        uint8_t* fuzzedBuf =
            fuzzer.postFuzzing(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(len), fuzzedSize);
        // => acum fuzzedBuf conține ( `[original][mutat de Radamsa]` ),
        //    iar fuzzedSize este lungimea totală a acestui buffer.

        // 3) Pregătim sockaddr_in pentru destinație și trimitem fuzzedBuf
        struct sockaddr_in dst_addr {};
        dst_addr.sin_family = AF_INET;
        dst_addr.sin_port   = htons(target_port);
        inet_pton(AF_INET, target_ip.c_str(), &dst_addr.sin_addr);

        uint8_t _UDPPayload[MAX_UDP_PAYLOAD_SIZE] = {0};
        size_t  _UDPPayloadSize = fuzzedSize > (MAX_UDP_PAYLOAD_SIZE - 1) ? (MAX_UDP_PAYLOAD_SIZE - 1) : fuzzedSize;
        memcpy(_UDPPayload, fuzzedBuf, _UDPPayloadSize);
        ssize_t sent = sendto(send_sock, _UDPPayload, _UDPPayloadSize, 0, reinterpret_cast<sockaddr*>(&dst_addr),
                              sizeof(dst_addr));
        if (sent < 0) {
            perror("[TYPE] [RECV THREAD] sendto failed");
        } else {
            std::cout << "[TYPE] [RECV THREAD] Sent " << sent << " bytes (fuzzed) to " << target_ip << ":"
                      << target_port << std::endl;
        }
        // 4) Dezalocăm bufferul fuzz-uit
        free(fuzzedBuf);
        // ==============================================

    } // end while

    std::cout << "[TYPE] [RECV THREAD] Closing recv socket FD: " << recv_sock << std::endl;
    close(recv_sock);
    return nullptr;
}

void* UDPHandler::sendThreadEntry(void* arg)
{
    // Extract parameters: the socket on which we listen and the associated UDPConnection object
    auto [send_sock, conn, isFromA] = *static_cast<std::tuple<int, UDPConnection*, bool>*>(arg);
    delete static_cast<std::tuple<int, UDPConnection*, bool>*>(arg);

    // 1) Retrieve the fuzzer instance from the singleton
    UDPHandler* handler = UDPHandler::getInstance();
    FuzzerCore& fuzzer  = handler->fuzzer;

    char               buffer[4096] = {0};
    struct sockaddr_in src_addr {};
    socklen_t          addrlen = sizeof(src_addr);

    std::cout << "[SEND-THREAD] Listening on FD " << send_sock << (isFromA ? " (from A side)" : " (from B side)")
              << std::endl;

    while (true) {
        // Receive data transparently on send_sock
        ssize_t len = recvfrom(send_sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&src_addr), &addrlen);
        if (len <= 0) {
            perror("[SEND-THREAD] recvfrom failed");
            break;
        }

        // Extract source IP and port
        char src_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, sizeof(src_ip));
        int src_port = ntohs(src_addr.sin_port);

        printf("[SEND-INFO] Received %zd bytes on send-sock FD %d from %s:%u\n", len, send_sock, src_ip, src_port);

        int         forward_sock = -1;
        std::string dst_ip;
        int         dst_port = -1;

        if (isFromA) {
            // Direction A -> B
            forward_sock = conn->getRecvSockFromEntityB();
            dst_ip       = conn->getEntityBIP();

            if (conn->getEntityBPort() == -1) {
                // Use a dynamic port for B from the list
                if (!conn->popDynamicPort(dst_port)) {
                    // If no more ports in the list, fall back to the source port
                    dst_port = src_port;
                    std::cout << "[SEND-WARN] (A->B) Dynamic port list empty, using source port: " << src_port
                              << std::endl;
                } else {
                    std::cout << "[SEND-INFO] (A->B) Popped dynamic port for B: " << dst_port << std::endl;
                }
            } else {
                // B has a static port
                dst_port = conn->getEntityBPort();
            }
        } else {
            // Direction B -> A
            forward_sock = conn->getRecvSockFromEntityA();
            dst_ip       = conn->getEntityAIP();

            if (conn->getEntityAPort() == -1) {
                // Use a dynamic port for A from the list
                if (!conn->popDynamicPort(dst_port)) {
                    dst_port = src_port;
                    std::cout << "[SEND-WARN] (B->A) Dynamic port list empty, using source port: " << src_port
                              << std::endl;
                } else {
                    std::cout << "[SEND-INFO] (B->A) Popped dynamic port for A: " << dst_port << std::endl;
                }
            } else {
                // A has a static port
                dst_port = conn->getEntityAPort();
            }
        }

        // === Insert fuzzer call (postFuzzing) here ===
        // Copy received data into a buffer and apply post-fuzzing
        size_t   fuzzedSize = 0;
        uint8_t* fuzzedBuf =
            fuzzer.postFuzzing(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(len), fuzzedSize);
        // =============================================

        struct sockaddr_in dst_addr {};
        dst_addr.sin_family = AF_INET;
        dst_addr.sin_port   = htons(dst_port);
        inet_pton(AF_INET, dst_ip.c_str(), &dst_addr.sin_addr);

        uint8_t _UDPPayload[MAX_UDP_PAYLOAD_SIZE] = {0};
        size_t  _UDPPayloadSize = fuzzedSize > (MAX_UDP_PAYLOAD_SIZE - 1) ? (MAX_UDP_PAYLOAD_SIZE - 1) : fuzzedSize;
        memcpy(_UDPPayload, fuzzedBuf, _UDPPayloadSize);
        // Send the fuzzed data onward
        ssize_t sent = sendto(forward_sock, _UDPPayload, _UDPPayloadSize, 0, reinterpret_cast<sockaddr*>(&dst_addr),
                              sizeof(dst_addr));
        if (sent < 0) {
            perror("[SEND-THREAD] sendto failed");
        } else {
            std::cout << "[SEND-DEBUG] Forwarded " << sent << " bytes (fuzzed) to " << dst_ip << ":" << dst_port
                      << " via FD " << forward_sock << std::endl;
        }

        // Free the fuzzed buffer
        free(fuzzedBuf);
    }

    std::cout << "[SEND-INFO] Closing send-sock FD: " << send_sock << std::endl;
    close(send_sock);
    return nullptr;
}

void UDPHandler::startSendThreads()
{
    std::cout << "[DEBUG] Launching sender threads for each send socket..." << std::endl;

    for (size_t idx = 0; idx < send_sockets_.size(); ++idx) {
        int sock = send_sockets_[idx];
        if (sock_to_connection_.count(sock) == 0) {
            std::cerr << "[ERROR] No mapping found for send-socket FD " << sock << std::endl;
            continue;
        }

        UDPConnection* conn = sock_to_connection_[sock];
        if (conn->getEntityAPort() != -1 && conn->getEntityBPort() != -1) {
            continue;
        }

        pthread_t   tid;
        bool        isFromA;
        sockaddr_in sock_addr{};
        socklen_t   len = sizeof(sock_addr);
        if (getsockname(sock, (sockaddr*) &sock_addr, &len) < 0) {
            perror("[ERROR] getsockname");
            isFromA = false; // fallback, dar ideal ar fi să nu ajungem aici
        } else {
            int bound_port = ntohs(sock_addr.sin_port);
            if (bound_port == conn->getEntityASendPort()) {
                // Portul pe care proxy-ul ascultă ca să trimită spre A,
                // a primit un pachet ⇒ pachet de la A spre B
                isFromA = true;
            } else {
                isFromA = false;
            }
        }

        // Pregătim argumentele pentru thread
        auto* args = new std::tuple<int, UDPConnection*, bool>(sock, conn, isFromA);
        if (pthread_create(&tid, nullptr, &UDPHandler::sendThreadEntry, args) != 0) {
            perror("[ERROR] pthread_create (sendThreadEntry) failed");
            delete args;
        } else {
            pthread_detach(tid);
            std::cout << "[DEBUG] Send-thread launched for socket FD: " << sock << (isFromA ? " (A→B)" : " (B→A)")
                      << std::endl;
        }
    }

    std::cout << "[INFO] All UDP send threads launched." << std::endl;
}
