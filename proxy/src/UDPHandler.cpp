#include "UDPHandler.hpp"

#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

UDPHandler* UDPHandler::instance_ = nullptr;
using Endpoint = UDPHandler::Endpoint;

UDPHandler::UDPHandler(const std::vector<utils::EntityConfig>& entities)
    : entities_(entities) {
    instance_ = this;
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
            addr.sin_family = AF_INET;
            addr.sin_port = htons(proxy_port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("[ERROR] bind");
                close(sock);
                continue;
            }

            recv_sockets_.push_back(sock);
            sock_to_connection_[sock] = conn;

            std::cout << "[UDPHandler] Bound recv socket on proxy port " << proxy_port
                      << " for " << conn.src_ip << ":" << conn.src_port
                      << " <-> " << conn.dst_ip << ":" << conn.dst_port << "\n";
        }
    }
}

static void* socketRecvThread(void* arg) {
    int sock = *static_cast<int*>(arg);
    delete static_cast<int*>(arg);

    const auto& sock_map = UDPHandler::getInstance()->sock_to_connection_;
    auto conn_it = sock_map.find(sock);
    if (conn_it != sock_map.end()) {
        const auto& conn = conn_it->second;
        printf("[THREAD] Started recv thread on sock %d for connection [%s:%d <-> %s:%d]\n",
               sock, conn.src_ip.c_str(), conn.src_port, conn.dst_ip.c_str(), conn.dst_port);
    } else {
        printf("[THREAD] Started recv thread on sock %d (connection unknown)\n", sock);
    }

    char buffer[4096];
    char cmsgbuf[512];

    struct sockaddr_in src_addr;
    struct iovec iov = { .iov_base = buffer, .iov_len = sizeof(buffer) };
    struct msghdr msg = {
        .msg_name = &src_addr,
        .msg_namelen = sizeof(src_addr),
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = cmsgbuf,
        .msg_controllen = sizeof(cmsgbuf),
        .msg_flags = 0
    };

    while (true) {
        ssize_t len = recvmsg(sock, &msg, 0);
        if (len < 0) {
            perror("[ERROR] recvmsg failed");
            continue;
        }

        char src_ip[INET_ADDRSTRLEN], dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(src_addr.sin_addr), src_ip, sizeof(src_ip));
        int src_port = ntohs(src_addr.sin_port);

        struct sockaddr_in orig_dst{};
        bool got_dst = false;

        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
             cmsg != nullptr;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
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

        printf("[RECV] [%s:%d → %s:%d] %zd bytes on sock %d\n",
               src_ip, src_port, dst_ip, dst_port, len, sock);
    }

    return nullptr;
}

void UDPHandler::startRecvThreads() {
    for (int sock : recv_sockets_) {
        pthread_t thread;
        int* sock_ptr = new int(sock);
        int ret = pthread_create(&thread, nullptr, socketRecvThread, sock_ptr);
        if (ret != 0) {
            perror("[ERROR] pthread_create failed");
            delete sock_ptr;
            continue;
        }
        threads_.push_back(thread);
    }

    printf("[INFO] Launched %zu UDP recv threads.\n", recv_sockets_.size());
}
