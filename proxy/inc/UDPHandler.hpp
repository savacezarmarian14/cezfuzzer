#ifndef UDP_HANDLER_HPP
#define UDP_HANDLER_HPP

#include <pthread.h>
#include <netinet/in.h>
#include <unordered_map>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

// Structura pentru o sesiune per client UDP
struct ClientSession {
    sockaddr_in client_addr;
    pthread_t thread;
    bool running;

    std::queue<std::vector<uint8_t>> message_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
};

// Clasa principală care gestionează toate sesiunile UDP
class UDPHandler {
public:
    UDPHandler(const char* listen_ip, int listen_port,
               const char* server_ip, int server_port);
    ~UDPHandler();

    void start();
    void stop();

private:
    static void* thread_entry(void* arg);
    void run();

    static void* client_session_thread(void* arg);

    std::string get_client_key(const sockaddr_in& addr);
    void create_client_session(const sockaddr_in& client_addr, const uint8_t* data, ssize_t len);
    void cleanup_inactive_sessions();

    // C-like config
    char listen_ip_[64];
    int listen_port_;
    char server_ip_[64];
    int server_port_;

    int sock_fd_;
    sockaddr_in server_addr_;

    pthread_t handler_thread_;
    bool running_;

    // sesiuni active: "ip:port" -> ClientSession*
    std::unordered_map<std::string, ClientSession*> client_sessions_;
    std::mutex sessions_mutex_;
};

#endif // UDP_HANDLER_HPP
