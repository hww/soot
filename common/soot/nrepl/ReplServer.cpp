#include "common/soot/nrepl/ReplServer.hpp"
#include "common/util/Log.hpp"
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

ReplServer::ReplServer(std::function<bool()> shutdown_callback, int port)
    : XSocketServer(shutdown_callback, port, 1024 * 1024) { // 1MB buffer
    header_buffer_.resize(sizeof(ReplMessageHeader));
}

void ReplServer::post_init() {
    lg::info("[nREPL:{}] Server initialized and listening", tcp_port);
}
std::optional<std::string> ReplServer::get_msg() {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    
    // Используем правильное имя из XSocketServer
    int server_fd = listening_socket; 
    if (server_fd == -1) return std::nullopt;

    FD_SET(server_fd, &read_fds);
    int max_fd = server_fd;

    for (int sock : client_sockets_) {
        FD_SET(sock, &read_fds);
        if (sock > max_fd) max_fd = sock;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000; 

    int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
    if (activity <= 0) return std::nullopt;

    // 1. Новое подключение
    if (FD_ISSET(server_fd, &read_fds)) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (new_socket >= 0) {
            handle_new_connection(new_socket);
        }
    }

    // 2. Чтение данных
    for (auto it = client_sockets_.begin(); it != client_sockets_.end(); ) {
        int sock = *it;
        if (FD_ISSET(sock, &read_fds)) {
            auto msg = handle_client_message(sock);
            if (msg.has_value()) {
                if (message_handler_) message_handler_(*msg, sock);
                return msg;
            }
            // Если handle_client_message вернул nullopt, 
            // предполагаем, что он сам обработал закрытие сокета или там пока нет данных.
            // Чтобы не ломать итератор в set, выходим из цикла и проверим в след. раз.
            return std::nullopt; 
        }
        ++it;
    }

    return std::nullopt;
}

void ReplServer::handle_new_connection(int new_socket) {
    lg::info("[nREPL:{}] New client connected: {}", tcp_port, new_socket);

    if (client_sockets_.size() < 50) { // max clients
        client_sockets_.insert(new_socket);
        ping_response(new_socket);
    }
    else {
        lg::warn("[nREPL:{}] Maximum clients reached, rejecting connection", tcp_port);
        error_response(new_socket, "Maximum clients reached");
        close_socket(new_socket);
    }
}

std::optional<std::string> ReplServer::handle_client_message(int socket) {
    char buffer[8192];
    // Читаем всё что пришло
    ssize_t n = recv(socket, buffer, sizeof(buffer) - 1, 0);

    if (n > 0) {
        buffer[n] = '\0';
        std::string msg(buffer);
        // Важно: убираем лишние символы конца строки для интерпретатора
        msg.erase(msg.find_last_not_of(" \n\r\t") + 1);
        return msg;
    } 
    
    if (n == 0) {
        lg::info("Client {} disconnected", socket);
        close(socket);
        client_sockets_.erase(socket);
        return std::nullopt;
    }

    // Если n < 0, проверяем, не пуст ли буфер (EAGAIN)
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        lg::error("Socket error {}: {}", socket, strerror(errno));
        close(socket);
        client_sockets_.erase(socket);
    }
    return std::nullopt;
}

void ReplServer::error_response(int socket, const std::string& error) {
    std::string msg = fmt::format("[ERROR]: {}", error);
    write_to_socket(socket, msg.c_str(), msg.size());
}

void ReplServer::ping_response(int socket) {
    std::string ping = "Connected to Aleste Lisp nREPL!";
    write_to_socket(socket, ping.c_str(), ping.size());
}