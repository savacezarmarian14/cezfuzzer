#include "UDPHandler.hpp"
#include "Fuzzer.hpp"

#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

/// TODO TO ADD IN LOGGER IN THE FUTURE
void printTextAndHex(const char* label, const uint8_t* data, size_t size) {
    printf("[%s] ", label);
    for (size_t i = 0; i < size; ++i) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    printf("[%s_TEXT] ", label);
    for (size_t i = 0; i < size; ++i) {
        if (data[i] >= 32 && data[i] <= 126)
            printf("%c", data[i]);
        else
            printf(".");
    }
    printf("\n\n");
}

UDPHandler* UDPHandler::instance_ = nullptr;
using Endpoint                    = UDPHandler::Endpoint;

UDPHandler::UDPHandler(const std::vector<utils::EntityConfig>& entities) : entities_(entities) {
    instance_ = this;
    fuzzer    = FuzzerCore(FUZZSTYLE_RANDOMIZATION);
}

UDPHandler* UDPHandler::getInstance() {
    return instance_;
}

void UDPHandler::buildFromConnections(const std::vector<utils::Connection>& connections) {
    for (const auto& conn : connections) {
        for (int proxy_port : {conn.port_src_proxy, conn.port_dst_proxy}) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) {
                perror("[ERROR] socket");
                continue;
            }

            int optval = 1;
            if (setsockopt(sock, SOL_IP, IP_RECVORIGDSTADDR, &optval, sizeof(optval)) < 0) {
                perror("[ERROR] setsockopt IP_RECVORIGDSTADDR");
                close(sock);
                continue;
            }

            sockaddr_in addr{};
            addr.sin_family      = AF_INET;
            addr.sin_port        = htons(proxy_port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(sock, (sockaddr*) &addr, sizeof(addr)) < 0) {
                perror("[ERROR] bind");
                close(sock);
                continue;
            }

            recv_sockets_.push_back(sock);
            sock_to_connection_[sock] = conn;

            std::cout << "[UDPHandler] Bound recv socket on proxy port " << proxy_port << " for " << conn.src_ip << ":"
                      << conn.src_port << " <-> " << conn.dst_ip << ":" << conn.dst_port << "\n";
        }
    }
}

static void* socketRecvThread(void* arg) {
    int sock = *static_cast<int*>(arg);
    delete static_cast<int*>(arg);

    const auto& sock_map = UDPHandler::getInstance()->sock_to_connection_;
    auto        conn_it  = sock_map.find(sock);
    if (conn_it == sock_map.end()) {
        printf("[THREAD] Unknown socket %d\n", sock);
        return nullptr;
    }

    const auto& conn = conn_it->second;
    printf("[THREAD] Started recv thread on sock %d for connection [%s:%d <-> %s:%d]\n", sock, conn.src_ip.c_str(),
           conn.src_port, conn.dst_ip.c_str(), conn.dst_port);

    char buffer[4096];
    char cmsgbuf[512];

    struct sockaddr_in src_addr;
    struct iovec       iov = {.iov_base = buffer, .iov_len = sizeof(buffer)};
    struct msghdr      msg = {.msg_name       = &src_addr,
                              .msg_namelen    = sizeof(src_addr),
                              .msg_iov        = &iov,
                              .msg_iovlen     = 1,
                              .msg_control    = cmsgbuf,
                              .msg_controllen = sizeof(cmsgbuf),
                              .msg_flags      = 0};

    while (true) {
        ssize_t len = recvmsg(sock, &msg, 0);
        if (len < 0) {
            perror("[ERROR] recvmsg failed");
            continue;
        }

        char src_ip[INET_ADDRSTRLEN], dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(src_addr.sin_addr), src_ip, sizeof(src_ip));
        int src_port = ntohs(src_addr.sin_port);

        struct sockaddr_in orig_dst {};
        bool               got_dst = false;

        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_ORIGDSTADDR) {
                memcpy(&orig_dst, CMSG_DATA(cmsg), sizeof(orig_dst));
                got_dst = true;
                break;
            }
        }

        if (!got_dst) {
            printf("[WARN] Missing ORIGDSTADDR on sock %d\n", sock);
            continue;
        }

        inet_ntop(AF_INET, &orig_dst.sin_addr, dst_ip, sizeof(dst_ip));
        int dst_port = ntohs(orig_dst.sin_port);

        printf("[RECV] [%s:%d → %s:%d] %zd bytes on sock %d\n", src_ip, src_port, dst_ip, dst_port, len, sock);

        // Construim payload modificat
        std::string payload = "[PROXY] ";
        payload.append(buffer, len);

        // Determinăm direcția: dacă mesajul vine de la A, trimitem către B
        std::string target_ip;
        int         target_port;

        if (src_ip == conn.src_ip && src_port == conn.src_port) {
            target_ip   = conn.dst_ip;
            target_port = conn.dst_port;
        } else {
            target_ip   = conn.src_ip;
            target_port = conn.src_port;
        }

        // Creăm socket temporar pentru trimitere
        int send_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (send_sock < 0) {
            perror("[ERROR] socket (sendto)");
            continue;
        }

        struct sockaddr_in target_addr {};
        target_addr.sin_family = AF_INET;
        target_addr.sin_port   = htons(target_port);
        inet_pton(AF_INET, target_ip.c_str(), &target_addr.sin_addr);

        size_t   fuzzed_payload_len = 0;
        uint8_t* _uint8_payload     = new uint8_t[MAX_BUFFER_SIZE];

        memcpy(_uint8_payload, payload.data(), payload.size());

        uint8_t* fuzzed_payload =
            UDPHandler::getInstance()->fuzzer.fullFuzzing(_uint8_payload, payload.size(), fuzzed_payload_len);
        delete[] _uint8_payload;
        ssize_t sent = sendto(send_sock, fuzzed_payload, fuzzed_payload_len, 0, (struct sockaddr*) &target_addr,
                              sizeof(target_addr));
        if (sent < 0) {
            perror("[ERROR] sendto");
        } else {
            printf("[FORWARD] → %s:%d (%zd bytes)\n", target_ip.c_str(), target_port, sent);
        }

        close(send_sock); // închidem socket-ul temporar
    }

    return nullptr;
}

void UDPHandler::startRecvThreads() {
    for (int sock : recv_sockets_) {
        pthread_t thread;
        int*      sock_ptr = new int(sock);
        int       ret      = pthread_create(&thread, nullptr, socketRecvThread, sock_ptr);
        if (ret != 0) {
            perror("[ERROR] pthread_create failed");
            delete sock_ptr;
            continue;
        }
        threads_.push_back(thread);
    }

    printf("[INFO] Launched %zu UDP recv threads.\n", recv_sockets_.size());
}
