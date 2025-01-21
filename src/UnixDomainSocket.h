#ifndef __UNIX_DOMAIN_SOCKET_H__
#define __UNIX_DOMAIN_SOCKET_H__


#include <iostream>
#include <mutex>
#include <vector>
#include <functional>

class UnixDomainSocket {
 public:
    UnixDomainSocket(std::string path_address, bool is_uds = true);
    ~UnixDomainSocket();

    bool CreateServer(uint16_t port = 0);

    bool ConnectServer(uint16_t port = 0);

    bool ClientSendMsg(const char* data, int len);

    bool ServerSendMsg(const char* data, int len);

    int GetClientSockfd() {
        return sockfd_client_;
    }

    void AddReadServerDataCallBack(std::function<void(const uint8_t *, const uint32_t)> handler) {
        read_from_server_function_ = handler;
    }

    void AddReadClientDataCallBack(std::function<void(const uint8_t *, const uint32_t)> handler) {
        read_from_client_function_ = handler;
    }

    bool ClientSockfdClose();

private:
    std::string path_address_;
    int sockfd_server_;
    int sockfd_client_;
    std::vector<int> sockfd_list_;
    bool is_uds_; // false: ip地址，true：unix domain socket
    std::function<void(const uint8_t *, const uint32_t)> read_from_server_function_; // 接收回调
    std::function<void(const uint8_t *, const uint32_t)> read_from_client_function_; // 接收回调
    std::function<void(const uint8_t *, const uint32_t)> accept_client_function_; // 接收回调

    void ReadServerDataCallBack(int sockfd);

    void ReadClientDataCallBack(int sockfd);

    void AcceptClientCallBack(int sockfd);
};

#endif
