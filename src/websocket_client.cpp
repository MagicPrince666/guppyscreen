/*
 * websocket client
 *
 * @build   make examples
 * @server  bin/websocket_server_test 8888
 * @client  bin/websocket_client_test ws://127.0.0.1:8888/test
 * @clients bin/websocket_client_test ws://127.0.0.1:8888/test 100
 * @python  scripts/websocket_server.py
 * @js      html/websocket_client.html
 *
 */

#include "websocket_client.h"
#include "spdlog/spdlog.h"
#include "utils.h"

#include <algorithm>

using namespace hv;
using json = nlohmann::json;

GcodeTransmitClient::GcodeTransmitClient(EventLoopPtr loop)
    : WebSocketClient(loop), id(0)
{
    uds_buffer_.size = 0;
}

GcodeTransmitClient::~GcodeTransmitClient()
{
}

int GcodeTransmitClient::connect(const char *url,
                                 std::function<void()> connected,
                                 std::function<void()> disconnected)
{
    spdlog::debug("websocket connecting to {}", url);

    if (KUtils::FileExists(url)) { // 本地通讯检查
        uds_module_ = std::make_shared<UnixDomainSocket>(url, true);
    }
    if (uds_module_) {
        connected_    = connected;
        disconnected_ = disconnected;
        uds_module_->AddReadServerDataCallBack(std::bind(&GcodeTransmitClient::RecvUdsServerBuffer, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        // set callbacks
        onopen = [this, connected]() {
            const HttpResponsePtr &resp = getHttpResponse();
            spdlog::info("onopen {}", resp->body.c_str());
            connected();
        };

        onmessage = [this, connected, disconnected](const std::string &msg) {
            // if (msg.find("notify_proc_stat_update") == std::string::npos) {
            //   spdlog::trace("onmessage(type={} len={}): {}", opcode() == WS_OPCODE_TEXT ? "text" : "binary",
            // 	     (int)msg.size(), msg);
            // }
            auto j = json::parse(msg);

            if (j.contains("id")) {
                // XXX: get rid of consumers and use function ptrs for callback
                const auto &entry = consumers.find(j["id"]);
                if (entry != consumers.end()) {
                    entry->second->consume(j);
                    consumers.erase(entry);
                }

                const auto &cb_entry = callbacks.find(j["id"]);
                if (cb_entry != callbacks.end()) {
                    cb_entry->second(j);
                    callbacks.erase(cb_entry);
                }
            }

            if (j.contains("method")) {
                std::string method = j["method"].template get<std::string>();
                if ("notify_status_update" == method) {
                    for (const auto &entry : notify_consumers) {
                        entry->consume(j);
                    }
                } //  else if ("notify_gcode_response" == method) {
                // 	for (const auto &gcode_cb : gcode_resp_cbs) {
                // 	  gcode_cb(j);
                // 	}
                // }
                else if ("notify_klippy_disconnected" == method) {
                    disconnected();
                } else if ("notify_klippy_ready" == method) {
                    connected();
                }

                for (const auto &entry : method_resp_cbs) {
                    if (method == entry.first) {
                        for (const auto &handler_entry : entry.second) {
                            handler_entry.second(j);
                        }
                    }
                }
            }
        };
    }

    onclose = [disconnected]() {
        // spdlog::debug("onclose");
        disconnected();
    };

    if (uds_module_) {
        if (!uds_module_->ConnectServer()) {
            spdlog::error("unix domain socket connecting failed!");
            uds_module_ = nullptr;
            return -1;
        }
        connected_();
        return uds_module_->GetClientSockfd();
    }

    // ping
    setPingInterval(10000);

    reconn_setting_t reconn;
    reconn_setting_init(&reconn);
    reconn.min_delay    = 200;
    reconn.max_delay    = 2000;
    reconn.delay_policy = 2;
    setReconnect(&reconn);

    http_headers headers;
    return open(url, headers);
};

int GcodeTransmitClient::send_jsonrpc(const std::string &method,
                                      const json &params,
                                      std::function<void(json &)> cb)
{
    const auto &entry = callbacks.find(id);
    if (entry == callbacks.end()) {
        // spdlog::debug("registering consume %d, %x\n", id, consumer);
        callbacks.insert({id, cb});
        // XXX: check success, remove consumer if send is unsuccessfull
        return send_jsonrpc(method, params);
    } else {
        // spdlog::debug("WARN: id %d is already register with a consumer\n", id);
    }

    return 0;
}

int GcodeTransmitClient::send_jsonrpc(const std::string &method,
                                      std::function<void(json &)> cb)
{
    const auto &entry = callbacks.find(id);
    if (entry == callbacks.end()) {
        // spdlog::debug("registering consume %d, %x\n", id, consumer);
        callbacks.insert({id, cb});
        // XXX: check success, remove consumer if send is unsuccessfull
        return send_jsonrpc(method);
    } else {
        // spdlog::debug("WARN: id %d is already register with a consumer\n", id);
    }

    return 0;
}

int GcodeTransmitClient::send_jsonrpc(const std::string &method, const json &params, NotifyConsumer *consumer)
{
    const auto &entry = consumers.find(id);
    if (entry == consumers.end()) {
        consumers.insert({id, consumer});
        return send_jsonrpc(method, params);
    }
    return 0;
}

void GcodeTransmitClient::register_notify_update(NotifyConsumer *consumer)
{
    if (std::find(notify_consumers.begin(), notify_consumers.end(), consumer) == std::end(notify_consumers)) {
        notify_consumers.push_back(consumer);
    }
}

void GcodeTransmitClient::unregister_notify_update(NotifyConsumer *consumer)
{
    notify_consumers.erase(std::remove_if(
        notify_consumers.begin(), notify_consumers.end(),
        [consumer](NotifyConsumer *c) {
            return c == consumer;
        }));
}

int GcodeTransmitClient::send_jsonrpc(const std::string &method,
                                      const json &params)
{
    json rpc;
    rpc["jsonrpc"] = "2.0";
    rpc["method"]  = method;
    rpc["params"]  = params;
    rpc["id"]      = id++;

    if (uds_module_) {
        std::string request = rpc.dump();
        request += "\x03";
        spdlog::debug("send_jsonrpc by uds: {}", request);
        return uds_module_->ClientSendMsg(request.c_str(), request.size());
    }
    spdlog::debug("send_jsonrpc: {}", rpc.dump());
    return send(rpc.dump());
}

int GcodeTransmitClient::send_jsonrpc(const std::string &method)
{
    json rpc;
    rpc["jsonrpc"] = "2.0";
    rpc["method"]  = method;
    rpc["id"]      = id++;

    if (uds_module_) {
        std::string request = rpc.dump();
        request += "\x03";
        spdlog::debug("send_jsonrpc by uds: {}", request);
        return uds_module_->ClientSendMsg(request.c_str(), request.size());
    }
    spdlog::debug("send_jsonrpc: {}", rpc.dump());
    return send(rpc.dump());
}

int GcodeTransmitClient::gcode_script(const std::string &gcode)
{
    json cmd = {{"script", gcode}};
    if (uds_module_) {
        return send_jsonrpc("gcode/script", cmd);
    }
    return send_jsonrpc("printer.gcode.script", cmd);
}

void GcodeTransmitClient::register_method_callback(std::string resp_method,
                                                   std::string handler_name,
                                                   std::function<void(json &)> cb)
{

    const auto &entry = method_resp_cbs.find(resp_method);
    if (entry == method_resp_cbs.end()) {
        spdlog::debug("registering new method {}, handler {}", resp_method, handler_name);
        std::map<std::string, std::function<void(json &)>> handler_map;
        handler_map.insert({handler_name, cb});
        method_resp_cbs.insert({resp_method, handler_map});
    } else {
        spdlog::debug("found existing resp_method {} with handlers, updating handler callback {}",
                      resp_method, handler_name);
        entry->second.insert({handler_name, cb});
    }
}

void GcodeTransmitClient::RecvUdsServerBuffer(const uint8_t *buffer, const int length)
{
    // std::unique_lock<std::mutex> lck(g_mtx_);
    int32_t buf_size = sizeof(uds_buffer_.rx_buffer) - uds_buffer_.size;
    if (buf_size >= length) {
        memcpy(uds_buffer_.rx_buffer + uds_buffer_.size, buffer, length);
        uds_buffer_.size += length; // 更新buff长度
    } else {
        memcpy(uds_buffer_.rx_buffer + uds_buffer_.size, buffer, buf_size);
        uds_buffer_.size += buf_size; // 更新buff长度
    }
    for (uint32_t i = 0; i < uds_buffer_.size; i++) {
        if (uds_buffer_.rx_buffer[i] == 0x03) { // json 结束符
            std::string result_str;
            result_str.assign((char *)(uds_buffer_.rx_buffer), i + 1);
            uint8_t buffer[sizeof(uds_buffer_.rx_buffer)];
            uds_buffer_.size = uds_buffer_.size - i - 1;
            memcpy(buffer, uds_buffer_.rx_buffer + i + 1, uds_buffer_.size);
            memcpy(uds_buffer_.rx_buffer, buffer, uds_buffer_.size);
            HandleCallback(result_str);
            // g_cv_.notify_all(); // 唤醒所有线程.
        }
    }
}

void GcodeTransmitClient::HandleCallback(std::string &method)
{
    size_t pos = method.find_last_not_of("\x03");
    if (pos != std::string::npos) {
        // 删除最后一个\x03
        method = method.substr(0, pos + 1);
    }

    spdlog::info("get_jsonrpc: {}", method);

    auto j = json::parse(method);

    if (j.contains("id")) {
        // XXX: get rid of consumers and use function ptrs for callback
        const auto &entry = consumers.find(j["id"]);
        if (entry != consumers.end()) {
            entry->second->consume(j);
            consumers.erase(entry);
        }

        const auto &cb_entry = callbacks.find(j["id"]);
        if (cb_entry != callbacks.end()) {
            cb_entry->second(j);
            callbacks.erase(cb_entry);
        }
    }

    if (j.contains("method")) {
        std::string method = j["method"].template get<std::string>();
        if ("notify_status_update" == method) {
            for (const auto &entry : notify_consumers) {
                entry->consume(j);
            }
        } //  else if ("notify_gcode_response" == method) {
        // 	for (const auto &gcode_cb : gcode_resp_cbs) {
        // 	  gcode_cb(j);
        // 	}
        // }
        else if ("notify_klippy_disconnected" == method) {
            spdlog::info("klippy disconnected");
            uds_module_->ClientSockfdClose();
            disconnected_();
        } else if ("notify_klippy_ready" == method) {
            if (uds_module_ && !uds_module_->ConnectServer()) {
                spdlog::error("failed to reconnect");
            }
            connected_();
        }

        for (const auto &entry : method_resp_cbs) {
            if (method == entry.first) {
                for (const auto &handler_entry : entry.second) {
                    handler_entry.second(j);
                }
            }
        }
    }
}
