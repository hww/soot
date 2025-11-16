#include "repl_client.h"
#include "repl_server.h"  // Добавляем для использования статических методов
#include "third_party/fmt/include/fmt/format.h"
#include <iostream>
#include <vector>  // Добавляем для std::vector

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

ReplClient::ReplClient() {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
}

ReplClient::~ReplClient() {
    disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool ReplClient::connect_to(const std::string& host, int port) {
    client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_ < 0) return false;

    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr_.sin_addr) <= 0) {
        struct hostent* he = gethostbyname(host.c_str());
        if (he == nullptr) {
            close_socket(client_socket_);
            return false;
        }
        addr_.sin_addr = *((struct in_addr*)he->h_addr);
    }

    if (connect(client_socket_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
        close_socket(client_socket_);
        return false;
    }

    connected_ = true;
    fmt::print("Connected to {}:{}\n", host, port);
    return true;
}

void ReplClient::disconnect() {
    if (client_socket_ >= 0) {
        close_socket(client_socket_);
        client_socket_ = -1;
    }
    connected_ = false;
}

void ReplClient::eval(const std::string& form) {
    if (!connected_) return;

    ReplMessageHeader header;
    header.length = static_cast<uint32_t>(form.length());
    header.type = ReplMessageType::EVAL;

    const char* header_ptr = reinterpret_cast<const char*>(&header);
    std::vector<char> buffer(header_ptr, header_ptr + sizeof(header));
    buffer.insert(buffer.end(), form.begin(), form.end());

    int result = write_to_socket(client_socket_, buffer.data(), static_cast<int>(buffer.size()));
    if (result == -1) {
        disconnect();
    }
}

std::string ReplClient::read_response() {
    if (!connected_) return "";

    char buffer[1024];
    int bytes_read = read_from_socket(client_socket_, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        return std::string(buffer);
    }

    return "";
}

// Используем статические методы из ReplServer
int ReplClient::close_socket(int socket) {
    return ReplServer::close_socket(socket);
}

int ReplClient::read_from_socket(int socket, char* buffer, int length) {
    return ReplServer::read_from_socket(socket, buffer, length);
}

int ReplClient::write_to_socket(int socket, const char* buffer, int length) {
    return ReplServer::write_to_socket(socket, buffer, length);
}