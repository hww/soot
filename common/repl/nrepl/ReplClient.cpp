// clang-format off
#include "ReplClient.h"

#include "cross_sockets/XSocket.hpp"
#include "versions/version.h"

#include "fmt/core.h"

#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif
// clang-format on

void ReplClient::eval_no_response(const std::string &form) {
    if (!is_connected()) { return; }

    ReplServerHeader header;
    header.length = static_cast<u32>(form.size());
    header.type = ReplServerMessageType::EVAL;

    std::vector<char> buffer;
    buffer.resize(sizeof(header));
    std::memcpy(buffer.data(), &header, sizeof(header));
    buffer.insert(buffer.end(), form.begin(), form.end());

    int result = write_to_socket(client_socket, buffer.data(), static_cast<int>(buffer.size()));
    if (result == -1) { disconnect(); }
}

std::optional<std::string> ReplClient::eval(const std::string &form) {
    if (!is_connected()) { return std::nullopt; }

    // 1. Send the EVAL request.
    ReplServerHeader header;
    header.length = static_cast<u32>(form.size());
    header.type = ReplServerMessageType::EVAL;

    std::vector<char> request;
    request.resize(sizeof(header));
    std::memcpy(request.data(), &header, sizeof(header));
    request.insert(request.end(), form.begin(), form.end());

    if (write_to_socket(client_socket, request.data(), static_cast<int>(request.size())) == -1) {
        disconnect();
        return std::nullopt;
    }

    // 2. Read the header of the server's response.
    //
    // We expect exactly one EVAL_RESULT reply per EVAL request.
    // Anything else (server disconnect, garbage bytes, PING reply) is
    // treated as a failure.
    ReplServerHeader resp_header;
    int              header_got = 0;
    while (header_got < (int)sizeof(resp_header)) {
        auto bytes =
            read_from_socket(client_socket, reinterpret_cast<char *>(&resp_header) + header_got,
                             sizeof(resp_header) - header_got);
        if (bytes <= 0) {
            disconnect();
            return std::nullopt;
        }
        header_got += bytes;
    }

    if (resp_header.type != ReplServerMessageType::EVAL_RESULT) {
        // The server sent something unexpected (for example a welcome
        // PING because we connected but nobody was listening yet).
        // Give up rather than trying to resync the stream.
        return std::nullopt;
    }

    // 3. Read the payload.
    std::string result(resp_header.length, '\0');
    int         payload_got = 0;
    while (payload_got < (int)resp_header.length) {
        auto bytes = read_from_socket(client_socket, result.data() + payload_got,
                                      resp_header.length - payload_got);
        if (bytes <= 0) {
            disconnect();
            return std::nullopt;
        }
        payload_got += bytes;
    }

    return result;
}