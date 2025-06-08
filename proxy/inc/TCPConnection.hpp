#ifndef TCP_CONNECTION_HPP
#define TCP_CONNECTION_HPP

#include <string>
#include <netinet/in.h>

class TCP_Connection {
  public:
    TCP_Connection();
    TCP_Connection(int fd);
    TCP_Connection(int fd, std::string ip, uint16_t port);

    ~TCP_Connection();

    int         getFD() const;
    std::string getIP() const;
    uint16_t    getPort() const;

    void setFD(int fd);
    void setIP(const std::string& ip);
    void setPort(uint16_t port);
    void startConnectionThread(int forward_fd);

    static void* _connection_thread_loop(void* args);

  private:
    int         socket_fd_;
    std::string ip_;
    uint16_t    port_;
};

class TCP_ChannelPair {
  public:
    TCP_ChannelPair();
    ~TCP_ChannelPair();

    TCP_Connection& getClientSide(); // conexiune între client și proxy
    TCP_Connection& getServerSide(); // conexiune între proxy și server

    void setClientSide(const TCP_Connection& conn);
    void setServerSide(const TCP_Connection& conn);
    void startChannelThreads();

  private:
    TCP_Connection client_side_;
    TCP_Connection server_side_;
};

#endif // TCP_CONNECTION_HPP
