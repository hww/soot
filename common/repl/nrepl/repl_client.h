#pragma once

#include <string>
#include <vector>  // Добавляем для std::vector

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

// Убираем повторные определения - используем из repl_server.h
class ReplClient {
public:
    ReplClient();
    ~ReplClient();

    bool connect_to(const std::string& host = "127.0.0.1", int port = 8181);
    void disconnect();
    bool is_connected() const { return connected_; }

    void eval(const std::string& form);
    std::string read_response();

private:
    int client_socket_ = -1;
    bool connected_ = false;

#ifdef _WIN32
    SOCKADDR_IN addr_ = {};
#else
    struct sockaddr_in addr_ = {};
#endif

    // Приватные методы для работы с сокетами
    static int close_socket(int socket);
    static int read_from_socket(int socket, char* buffer, int length);
    static int write_to_socket(int socket, const char* buffer, int length);
};