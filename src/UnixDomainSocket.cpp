#include "UnixDomainSocket.h"
#include "utils.h"
#include "xepoll.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

UnixDomainSocket::UnixDomainSocket(std::string path_address, bool is_uds)
    : path_address_(path_address), sockfd_server_(-1), sockfd_client_(-1), is_uds_(is_uds)
{
}

UnixDomainSocket::~UnixDomainSocket()
{
    for (uint32_t i = 0; i < sockfd_list_.size(); i++) {
        MY_EPOLL.EpollDel(sockfd_list_[i]);
        close(sockfd_list_[i]);
    }
    if (sockfd_server_ > 0) {
        MY_EPOLL.EpollDel(sockfd_server_);
        close(sockfd_server_);
        unlink(path_address_.c_str());
    }
    if (sockfd_client_ > 0) {
        MY_EPOLL.EpollDel(sockfd_client_);
        close(sockfd_client_);
    }
}

bool UnixDomainSocket::CreateServer(uint16_t port)
{
    if (is_uds_) {
        sockfd_server_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd_server_ == -1) {
            perror("socket");
            return false;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        if (KUtils::FileExists(path_address_)) {
            remove(path_address_.c_str());
        }

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path_address_.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(sockfd_server_, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
            perror("bind");
            return false;
        }
    } else {
        sockfd_server_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_server_ == -1) {
            perror("socket");
            return false;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);

        if (bind(sockfd_server_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("bind");
            return false;
        }
    }

    if (listen(sockfd_server_, 5) == -1) {
        perror("listen");
        return false;
    }

    std::cout << "Server is listening on " << path_address_ << std::endl;

    MY_EPOLL.EpollAddRead(sockfd_server_, std::bind(&UnixDomainSocket::AcceptClientCallBack, this, std::placeholders::_1));

    return true;
}

bool UnixDomainSocket::ConnectServer(uint16_t port)
{
    if (is_uds_) {
        sockfd_client_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd_client_ == -1) {
            perror("socket");
            return false;
        }

        struct sockaddr_un serverAddr;
        memset(&serverAddr, 0, sizeof(struct sockaddr_un));
        serverAddr.sun_family = AF_UNIX;
        strncpy(serverAddr.sun_path, path_address_.c_str(), sizeof(serverAddr.sun_path) - 1);

        if (connect(sockfd_client_, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_un)) == -1) {
            // perror("connect");
            return false;
        }
    } else {
        sockfd_client_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_client_ == -1) {
            perror("socket");
            return false;
        }

        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(struct sockaddr_in));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port   = htons(port);
        // 将服务器地址转换为二进制形式
        if (inet_pton(AF_INET, path_address_.c_str(), &serverAddr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported" << std::endl;
            return false;
        }

        if (connect(sockfd_client_, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_un)) == -1) {
            // perror("connect");
            return false;
        }
    }

    MY_EPOLL.EpollAddRead(sockfd_client_, std::bind(&UnixDomainSocket::ReadServerDataCallBack, this, std::placeholders::_1));

    return true;
}

bool UnixDomainSocket::ClientSendMsg(const char *data, int len)
{
    if (sockfd_client_ == -1) {
        std::cout << "client not connected" << std::endl;
        return false;
    }
    int32_t bytesSent = send(sockfd_client_, data, len, 0);
    if (bytesSent == -1) {
        perror("send");
        MY_EPOLL.EpollDel(sockfd_client_);
        close(sockfd_client_);
        sockfd_client_ = -1;
        return false;
    }
    return true;
}

bool UnixDomainSocket::ServerSendMsg(const char *data, int len)
{
    for (uint32_t i = 0; i < sockfd_list_.size(); i++) {
        int bytesSent = send(sockfd_list_[i], data, len, 0);
        if (bytesSent == -1) {
            perror("send");
            MY_EPOLL.EpollDel(sockfd_list_[i]);
            close(sockfd_list_[i]);
            auto iter = std::remove(sockfd_list_.begin(), sockfd_list_.end(), sockfd_list_[i]);
            sockfd_list_.erase(iter, sockfd_list_.end());
        }
    }
    return true;
}

void UnixDomainSocket::ReadServerDataCallBack(int sockfd)
{
    char buffer[1024];
    ssize_t bytesRead = read(sockfd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        buffer[bytesRead] = 0;
        if (read_from_server_function_) {
            read_from_server_function_((uint8_t *)buffer, bytesRead);
        } else {
            std::cout << buffer << std::endl;
        }
    } else {
        if (bytesRead == -1) {
            perror("read");
        }
        std::cout << "server " << sockfd << " close " << std::endl;
        close(sockfd);
        sockfd_client_ = -1;
    }
}

void UnixDomainSocket::ReadClientDataCallBack(int sockfd)
{
    char buffer[1024];
    ssize_t bytesRead = read(sockfd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        buffer[bytesRead] = 0;
        if (read_from_client_function_) {
            read_from_client_function_((uint8_t *)buffer, bytesRead);
        } else {
            std::cout << "Received data: " << buffer << std::endl;
        }
    } else {
        if (bytesRead == -1) {
            perror("read");
        }
        std::cout << "client " << sockfd << " close " << std::endl;
        auto iter = std::remove(sockfd_list_.begin(), sockfd_list_.end(), sockfd);
        sockfd_list_.erase(iter, sockfd_list_.end());
        MY_EPOLL.EpollDel(sockfd);
        close(sockfd);
    }
}

void UnixDomainSocket::AcceptClientCallBack(int sockfd)
{
    int client_socket = accept(sockfd, NULL, NULL);
    if (client_socket == -1) {
        perror("accept");
        return;
    }
    std::cout << "accept client: " << client_socket << std::endl;

    sockfd_list_.push_back(client_socket);

    MY_EPOLL.EpollAddRead(client_socket, std::bind(&UnixDomainSocket::ReadClientDataCallBack, this, std::placeholders::_1));
}

bool UnixDomainSocket::ClientSockfdClose()
{
    if (sockfd_client_ > 0) {
        MY_EPOLL.EpollDel(sockfd_client_);
        close(sockfd_client_);
        sockfd_client_ = -1;
        return true;
    }
    return false;
}
