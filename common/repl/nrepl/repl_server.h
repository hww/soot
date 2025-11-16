#pragma once

#include <optional>
#include <set>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <string>  // Добавляем для std::string

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#endif

enum class ReplMessageType : uint32_t {
    PING = 0,
    EVAL = 10,
    SHUTDOWN = 20
};

struct ReplMessageHeader {
    uint32_t length;
    ReplMessageType type;
};

class ReplServer {
public:
    ReplServer(int port);
    ~ReplServer();

    bool init();
    void shutdown();
    std::optional<std::string> get_msg();

    void set_exit_callback(std::function<bool()> callback) {
        want_exit_callback_ = callback;
    }

    void set_message_handler(std::function<void(const std::string&, int)> handler) {
        message_handler_ = handler;
    }

    bool is_initialized() const { return initialized_; }
    int get_port() const { return port_; }

    // Статические методы для работы с сокетами
    static int close_socket(int socket);
    static int read_from_socket(int socket, char* buffer, int length);
    static int write_to_socket(int socket, const char* buffer, int length);

private:
    static constexpr int MAX_CLIENTS = 50;
    int listening_socket_ = -1;
    int port_ = 0;
    bool initialized_ = false;

    std::vector<char> header_buffer_;
    fd_set read_sockets_;
    std::set<int> client_sockets_;
    std::function<bool()> want_exit_callback_;
    std::function<void(const std::string&, int)> message_handler_;

    void error_response(int socket, const std::string& error);
    void ping_response(int socket);
    bool want_exit_callback() const {
        return want_exit_callback_ ? want_exit_callback_() : false;
    }

    static int accept_socket(int socket, struct sockaddr* addr, socklen_t* addrlen);
    static std::string address_to_string(const struct sockaddr_in& addr);

#ifdef _WIN32
    SOCKADDR_IN addr_ = {};
#else
    struct sockaddr_in addr_ = {};
#endif
};