#pragma once

#include <string>
#include "common/cross_sockets/XSocketClient.hpp"

#include "common/repl/nrepl/ReplCommon.hpp"  // Добавляем общие определения

class ReplClient : public XSocketClient {
public:
    ReplClient(int port);
    ~ReplClient() = default;

    void eval(const std::string& form);
    std::string read_response();

private:
    std::vector<char> read_buffer_;
};