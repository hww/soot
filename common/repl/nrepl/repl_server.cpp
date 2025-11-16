#include "repl_server.h"
#include "third_party/fmt/include/fmt/format.h"
#include <iostream>
#include <thread>
#include <chrono>

ReplServer::ReplServer(int port) : port_(port) {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    header_buffer_.resize(sizeof(ReplMessageHeader));
}

ReplServer::~ReplServer() {
    for (int sock : client_sockets_) {
        close_socket(sock);
    }
    shutdown();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool ReplServer::init() {
    listening_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket_ < 0) return false;

    int opt = 1;
#ifdef _WIN32
    setsockopt(listening_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(listening_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = INADDR_ANY;
    addr_.sin_port = htons(port_);

    if (bind(listening_socket_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
        close_socket(listening_socket_);
        return false;
    }

    if (listen(listening_socket_, 10) < 0) {
        close_socket(listening_socket_);
        return false;
    }

    initialized_ = true;
    return true;
}

void ReplServer::shutdown() {
    if (listening_socket_ >= 0) {
        close_socket(listening_socket_);
        listening_socket_ = -1;
    }
    initialized_ = false;
}

std::optional<std::string> ReplServer::get_msg() {
    FD_ZERO(&read_sockets_);
    FD_SET(listening_socket_, &read_sockets_);
    int max_sd = listening_socket_;

    for (int sock : client_sockets_) {
        if (sock > 0) {
            FD_SET(sock, &read_sockets_);
            if (sock > max_sd) max_sd = sock;
        }
    }

    struct timeval timeout = { 0, 100000 };
    int activity = select(max_sd + 1, &read_sockets_, nullptr, nullptr, &timeout);

    if (activity < 0 && errno != EINTR) {
        return std::nullopt;
    }

    if (FD_ISSET(listening_socket_, &read_sockets_)) {
        socklen_t addr_len = sizeof(addr_);
        int new_socket = accept_socket(listening_socket_, (struct sockaddr*)&addr_, &addr_len);

        if (new_socket >= 0) {
            ping_response(new_socket);
            if (client_sockets_.size() < MAX_CLIENTS) {
                client_sockets_.insert(new_socket);
                fmt::print("Client connected: {}\n", address_to_string(addr_));
            }
            else {
                error_response(new_socket, "Maximum clients reached");
                close_socket(new_socket);
            }
        }
    }

    for (auto it = client_sockets_.begin(); it != client_sockets_.end();) {
        int sock = *it;
        if (FD_ISSET(sock, &read_sockets_)) {
            int bytes_read = read_from_socket(sock, header_buffer_.data(), header_buffer_.size());

            if (bytes_read <= 0) {
                close_socket(sock);
                it = client_sockets_.erase(it);
                continue;
            }

            ReplMessageHeader* header = reinterpret_cast<ReplMessageHeader*>(header_buffer_.data());
            std::vector<char> buffer(header->length);

            int total_read = 0;
            int tries = 0;
            bool socket_error = false;

            while (total_read < header->length && tries < 100) {
                if (want_exit_callback()) return std::nullopt;

                bytes_read = read_from_socket(sock, buffer.data() + total_read, header->length - total_read);

                if (bytes_read <= 0) {
                    socket_error = true;
                    break;
                }

                total_read += bytes_read;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                tries++;
            }

            if (socket_error) {
                close_socket(sock);
                it = client_sockets_.erase(it);
                continue;
            }

            switch (header->type) {
            case ReplMessageType::PING:
                ping_response(sock);
                break;
            case ReplMessageType::EVAL:
                if (message_handler_) {
                    message_handler_(std::string(buffer.data(), header->length), sock);
                }
                break;
            case ReplMessageType::SHUTDOWN:
                close_socket(sock);
                it = client_sockets_.erase(it);
                continue;
            }
        }
        ++it;
    }

    return std::nullopt;
}

void ReplServer::error_response(int socket, const std::string& error) {
    std::string msg = "[ERROR]: " + error;
    write_to_socket(socket, msg.c_str(), msg.size());
}

void ReplServer::ping_response(int socket) {
    std::string ping = "Connected to Aleste Lisp nREPL!";
    write_to_socket(socket, ping.c_str(), ping.size());
}

int ReplServer::close_socket(int socket) {
#ifdef _WIN32
    return closesocket(socket);
#else
    return close(socket);
#endif
}

int ReplServer::read_from_socket(int socket, char* buffer, int length) {
    return recv(socket, buffer, length, 0);
}

int ReplServer::write_to_socket(int socket, const char* buffer, int length) {
    return send(socket, buffer, length, 0);
}

int ReplServer::accept_socket(int socket, struct sockaddr* addr, socklen_t* addrlen) {
    return accept(socket, addr, addrlen);
}

std::string ReplServer::address_to_string(const struct sockaddr_in& addr) {
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), buffer, INET_ADDRSTRLEN);
    return std::string(buffer) + ":" + std::to_string(ntohs(addr.sin_port));
}