/**
 * @file UnixDomainSocket.h
 * @author 黄李全 (huangliquan@dreame.tech)
 * @brief uds通讯
 * @version 0.1
 * @date 2024-11-28
 * @copyright Copyright (c) {2024} 追觅科技有限公司版权所有
 */
#ifndef __UNIX_DOMAIN_SOCKET_H__
#define __UNIX_DOMAIN_SOCKET_H__

#include <iostream>
#include <vector>

class UnixDomainSocket {
 public:
    UnixDomainSocket(std::string path);
    ~UnixDomainSocket();

    bool CreateServer();

    bool ConnectServer();

    bool ClientSendMsg(const char* data, int len);

private:
    std::string uds_path_;
    int sockfd_server_;
    int sockfd_client_;
    std::vector<int> sockfd_list_;

    void ReadServerDataCallBack(int sockfd);

    void ReadClientDataCallBack(int sockfd);

    void AcceptClientCallBack(int sockfd);

    bool FileExists(const std::string name);
};

#endif
