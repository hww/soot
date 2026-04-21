#include "CommonTypes.hpp"
#include "common/soot/nrepl/ReplServer.hpp"
#include "common/util/Log.hpp"
#include <thread>
#include <chrono>
#include <fcntl.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#include <sys/types.h>

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
    
    // ВАЖНО: проверяем, что унаследованный listening_socket валиден
    lg::info("Checking server socket...");
    
    if (listening_socket == -1) {
        lg::error("CRITICAL: Server socket is -1 (not created)");
        return;
    }
    
    lg::info("Server socket fd: {}", listening_socket);
    
    // Можно попробовать получить информацию о сокете
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(listening_socket, (struct sockaddr*)&addr, &addr_len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
        lg::info("Socket bound to {}:{}", ip_str, ntohs(addr.sin_port));
    } else {
        lg::error("Cannot get socket info: {}", strerror(errno));
    }
}

std::optional<std::string> ReplServer::get_msg() {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    
    int server_fd = listening_socket; 
    if (server_fd == -1) {
        return std::nullopt;
    }

    FD_SET(server_fd, &read_fds);
    int max_fd = server_fd;

    // Копируем клиентские сокеты
    std::vector<int> client_sockets_copy(client_sockets_.begin(), client_sockets_.end());
    for (int sock : client_sockets_copy) {
        FD_SET(sock, &read_fds);
        if (sock > max_fd) max_fd = sock;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms

    int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
    
    if (activity < 0) {
        lg::error("Select error: {}", strerror(errno));
        return std::nullopt;
    }
    
    if (activity == 0) {
        // Таймаут
        return std::nullopt;
    }

    // ВАЖНОЕ ИЗМЕНЕНИЕ: Сначала обрабатываем новые подключения
    if (FD_ISSET(server_fd, &read_fds)) {
        lg::info("select() detected new connection on server socket {}", server_fd);
        
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        
        // Установим неблокирующий accept
        int flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
        
        int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        
        // Восстанавливаем флаги
        fcntl(server_fd, F_SETFL, flags);
        
        if (new_socket >= 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            lg::info("Accepted connection from {} on socket {}", client_ip, new_socket);
            
            handle_new_connection(new_socket);
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                lg::warn("accept() would block, retrying later");
            } else {
                lg::error("accept() failed: {}", strerror(errno));
            }
        }
    }

    // Затем обрабатываем данные от клиентов
    for (auto it = client_sockets_copy.begin(); it != client_sockets_copy.end(); ++it) {
        int sock = *it;
        if (FD_ISSET(sock, &read_fds)) {
            lg::info("Data available on client socket {}", sock);
            auto msg = handle_client_message(sock);
            if (msg.has_value()) {
                if (message_handler_) {
                    message_handler_(*msg, sock);
                }
                return msg;
            }
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
    std::string ping = "Connected to Aleste Lisp nREPL!\n";
    lg::info("Sending welcome message to socket {}", socket);
    
    ssize_t sent = send(socket, ping.c_str(), ping.size(), 0);
    if (sent != static_cast<ssize_t>(ping.size())) {
        lg::error("Failed to send welcome message to socket {}: {}", socket, strerror(errno));
    } else {
        lg::info("Welcome message sent successfully to socket {}", socket);
    }
}