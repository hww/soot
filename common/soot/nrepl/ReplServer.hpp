#pragma once

#include <optional>
#include <set>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

#include "common/cross_sockets/XSocketServer.hpp"
#include "third_party/fmt/include/fmt/format.h"

#include "common/soot/nrepl/ReplCommon.hpp" 

class ReplServer : public XSocketServer {
public:
    ReplServer(std::function<bool()> shutdown_callback, int port);
    virtual ~ReplServer() = default;

    void post_init() override;
    std::optional<std::string> get_msg();

    void set_message_handler(std::function<void(const std::string&, int)> handler) {
        message_handler_ = handler;
    }

private:
    void handle_new_connection(int new_socket);
    std::optional<std::string> handle_client_message(int socket);
    void error_response(int socket, const std::string& error);
    void ping_response(int socket);

    std::vector<char> header_buffer_;
    std::set<int> client_sockets_;
    std::function<void(const std::string&, int)> message_handler_;
};