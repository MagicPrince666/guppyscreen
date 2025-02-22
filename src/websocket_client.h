#ifndef __KWEBSOCKET_CLIENT_H__
#define __KWEBSOCKET_CLIENT_H__

#include "WebSocketClient.h"
#include "UnixDomainSocket.h"
#include "json.hpp"
#include "notify_consumer.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

using json = nlohmann::json;

class GcodeTransmitClient  : public hv::WebSocketClient
{
public:
    GcodeTransmitClient(hv::EventLoopPtr loop);
    ~GcodeTransmitClient();

    int connect(const char *url,
                std::function<void()> connected,
                std::function<void()> disconnected);

    void register_notify_update(NotifyConsumer *consumer);
    void unregister_notify_update(NotifyConsumer *consumer);

    // void register_gcode_resp(std::function<void(json&)> cb);

    int send_jsonrpc(const std::string &method, std::function<void(json &)> cb);
    int send_jsonrpc(const std::string &method, const json &params, std::function<void(json &)> cb);
    int send_jsonrpc(const std::string &method, const json &params, NotifyConsumer *consumer);
    int send_jsonrpc(const std::string &method, const json &params);
    int send_jsonrpc(const std::string &method);
    int gcode_script(const std::string &gcode);

    void register_method_callback(std::string resp_method,
                                  std::string handler_name,
                                  std::function<void(json &)> cb);

private:
    std::map<uint32_t, std::function<void(json &)>> callbacks;
    std::map<uint32_t, NotifyConsumer *> consumers;
    std::vector<NotifyConsumer *> notify_consumers;
    // std::vector<std::function<void(json&)>> gcode_resp_cbs;

    // method_name : { <unique-name-cb-handler> :handler-cb }
    std::map<std::string, std::map<std::string, std::function<void(json &)>>> method_resp_cbs;
    std::atomic<uint64_t> id;
    std::shared_ptr<UnixDomainSocket> uds_module_;
    std::function<void()> connected_;
    std::function<void()> disconnected_;
    std::condition_variable g_cv_; // 全局条件变量
    std::mutex g_mtx_;             // 全局互斥锁.
    struct
    {
        uint8_t rx_buffer[2048];
        uint32_t size; // buf长度
    } uds_buffer_;

    /**
     * @brief uds接收klipper服务端回调
     * @param buffer
     * @param length
     */
    void RecvUdsServerBuffer(const uint8_t *buffer, const int length);

    void HandleCallback(std::string &method);
};

#endif //__KWEBSOCKET_CLIENT_H__
