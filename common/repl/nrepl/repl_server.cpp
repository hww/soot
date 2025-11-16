#include "repl_server.h"
#include "common/log/log.h"
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
    // Используем select для проверки активности с таймаутом
    fd_set read_sockets;
    FD_ZERO(&read_sockets);
    FD_SET(listening_socket, &read_sockets);

    int max_sd = listening_socket;
    for (int sock : client_sockets_) {
        if (sock > 0) {
            FD_SET(sock, &read_sockets);
            if (sock > max_sd) max_sd = sock;
        }
    }

    struct timeval timeout = { 0, 100000 }; // 100ms timeout
    int activity = select(max_sd + 1, &read_sockets, nullptr, nullptr, &timeout);

    if (activity < 0 && errno != EINTR) {
        lg::error("[nREPL:{}] select error: {}", tcp_port, strerror(errno));
        return std::nullopt;
    }

    // Проверяем новые подключения
    if (FD_ISSET(listening_socket, &read_sockets)) {
        socklen_t addr_len = sizeof(addr);
        int new_socket = accept_socket(listening_socket, (sockaddr*)&addr, &addr_len);
        if (new_socket >= 0) {
            handle_new_connection(new_socket);
        }
    }

    // Проверяем сообщения от клиентов
    for (auto it = client_sockets_.begin(); it != client_sockets_.end();) {
        int sock = *it;
        if (FD_ISSET(sock, &read_sockets)) {
            auto message = handle_client_message(sock);
            if (message) {
                return message;
            }
            // Если сокет закрыт, продолжаем итерацию
            ++it;
        }
        else {
            ++it;
        }
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
    // Читаем заголовок
    int bytes_read = read_from_socket(socket, header_buffer_.data(), header_buffer_.size());
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            lg::info("[nREPL:{}] Client disconnected: {}", tcp_port, socket);
        }
        else {
            lg::warn("[nREPL:{}] Error reading from socket {}: {}", tcp_port, socket, strerror(errno));
        }
        close_socket(socket);
        client_sockets_.erase(socket);
        return std::nullopt;
    }

    // Парсим заголовок
    ReplMessageHeader* header = reinterpret_cast<ReplMessageHeader*>(header_buffer_.data());
    std::vector<char> body_buffer(header->length);

    // Читаем тело сообщения (с partial read handling)
    int total_read = 0;
    int tries = 0;
    while (total_read < header->length && tries < 100) {
        if (want_exit_callback()) {
            lg::warn("[nREPL:{}] Server shutting down", tcp_port);
            return std::nullopt;
        }

        bytes_read = read_from_socket(socket, body_buffer.data() + total_read, header->length - total_read);
        if (bytes_read <= 0) {
            lg::warn("[nREPL:{}] Client disconnected during message read: {}", tcp_port, socket);
            close_socket(socket);
            client_sockets_.erase(socket);
            return std::nullopt;
        }

        total_read += bytes_read;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        tries++;
    }

    if (total_read < header->length) {
        lg::error("[nREPL:{}] Incomplete message from client {}", tcp_port, socket);
        return std::nullopt;
    }

    std::string message(body_buffer.data(), header->length);
    lg::debug("[nREPL:{}] Received message from client {}: {}", tcp_port, socket, message);

    // Обрабатываем тип сообщения
    switch (header->type) {
    case ReplMessageType::PING:
        ping_response(socket);
        break;
    case ReplMessageType::EVAL:
        if (message_handler_) {
            message_handler_(message, socket);
        }
        return std::make_optional(message);
    case ReplMessageType::SHUTDOWN:
        close_socket(socket);
        client_sockets_.erase(socket);
        break;
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