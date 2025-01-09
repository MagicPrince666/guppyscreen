#include "UnixDomainSocket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include "xepoll.h"
#include "utils.h"

UnixDomainSocket::UnixDomainSocket(std::string path)
: uds_path_(path), sockfd_server_(-1), sockfd_client_(-1)
{}

UnixDomainSocket::~UnixDomainSocket()
{
    for (uint32_t i = 0; i < sockfd_list_.size(); i++) {
        MY_EPOLL.EpollDel(sockfd_list_[i]);
        close(sockfd_list_[i]);
    }
    if (sockfd_server_ > 0) {
        MY_EPOLL.EpollDel(sockfd_server_);
        close(sockfd_server_);
        unlink(uds_path_.c_str());
    }
    if (sockfd_client_ > 0) {
        MY_EPOLL.EpollDel(sockfd_client_);
        close(sockfd_client_);
    }
}

bool UnixDomainSocket::CreateServer()
{
    sockfd_server_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd_server_ == -1) {
        perror("socket");
        return false;
    }

    if (FileExists(uds_path_)) {
        remove(uds_path_.c_str());
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, uds_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(sockfd_server_, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        return false;
    }

    if (listen(sockfd_server_, 5) == -1) {
        perror("listen");
        return false;
    }

    std::cout << "Server is listening on " << uds_path_ << std::endl;

    MY_EPOLL.EpollAddRead(sockfd_server_, std::bind(&UnixDomainSocket::AcceptClientCallBack, this, std::placeholders::_1));

    return true;
}

bool UnixDomainSocket::ConnectServer()
{
    sockfd_client_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd_client_ == -1) {
        perror("socket");
        return false;
    }

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, uds_path_.c_str(), sizeof(serverAddr.sun_path) - 1);

    if (connect(sockfd_client_, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        return false;
    }

    MY_EPOLL.EpollAddRead(sockfd_client_, std::bind(&UnixDomainSocket::ReadServerDataCallBack, this, std::placeholders::_1));

    return true;
}

bool UnixDomainSocket::ClientSendMsg(const char* data, int len)
{
    if (sockfd_client_ == -1) {
        std::cout << "client not connected" << std::endl;
        return false;
    }
    int32_t bytesSent = send(sockfd_client_, data, len, 0);
    if (bytesSent == -1) {
        perror("send");
        return false;
    } else {
        std::cout << "Message sent to server" << std::endl;
    }
    return true;
}

void UnixDomainSocket::ReadServerDataCallBack(int sockfd)
{
    char buffer[1024];
    ssize_t bytesRead = read(sockfd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        buffer[bytesRead] = 0;
        std::cout << "Received data from server: " << buffer << std::endl;
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
        std::cout << "Received data: " << buffer << std::endl;
    } else {
        if (bytesRead == -1) {
            perror("read");
        }
        std::cout << "client " << sockfd << " close " << std::endl;
        auto iter = std::remove(sockfd_list_.begin(), sockfd_list_.end(), sockfd);
        sockfd_list_.erase(iter, sockfd_list_.end());
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

bool UnixDomainSocket::FileExists(const std::string name)
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}