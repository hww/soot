// clang-format off
#include "ReplServer.h"

#include "cross_sockets/XSocket.hpp"
#include "versions/version.h"

#include "fmt/core.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif
#include "util/Log.hpp"
// clang-format on

// TODO - The server also needs to eventually return the result of the evaluation

ReplServer::~ReplServer() {
  // Close all our client sockets!
  for (const int& sock : client_sockets) {
    close_socket(sock);
  }
}

void ReplServer::post_init() {
  // Add the listening socket to our set of sockets
  lg::debug("[nREPL:{}:{}] awaiting connections", tcp_port, listening_socket);
}

void ReplServer::error_response(int socket, const std::string& error) {
  std::string msg = fmt::format("[ERROR]: {}", error);
  auto resp = write_to_socket(socket, msg.c_str(), msg.size());
  if (resp == -1) {
    lg::warn("[nREPL:{}] Client Disconnected: {}", tcp_port, address_to_string(addr),
             ntohs(addr.sin_port), socket);
    close_socket(socket);
    client_sockets.erase(socket);
  }
}

void ReplServer::ping_response(int socket) {
  std::string ping = fmt::format("Connected to SOOT v{}.{} nREPL!",
                                 versions::SOOT_VERSION_MAJOR, versions::SOOT_VERSION_MINOR);
  auto resp = write_to_socket(socket, ping.c_str(), ping.size());
  if (resp == -1) {
    lg::warn("[nREPL:{}] Client Disconnected: {}", tcp_port, address_to_string(addr),
             ntohs(addr.sin_port), socket);
    close_socket(socket);
    client_sockets.erase(socket);
  }
}

std::optional<ReplServerMessage> ReplServer::get_msg(int timeout_us) {
    // Clear the sockets we are listening on
    FD_ZERO(&read_sockets);

    // Add the server's main listening socket (where we accept clients from)
    FD_SET(listening_socket, &read_sockets);
    int max_sd = listening_socket;
    for (const int &sock : client_sockets) {
        if (sock > max_sd) { max_sd = sock; }
        if (sock > 0) { FD_SET(sock, &read_sockets); }
    }

    // Wait for activity on _something_, with a timeout so we don't get stuck here on exit.
    struct timeval timeout = {0, 100000};
    timeout.tv_sec = timeout_us / 1000000;
    timeout.tv_usec = timeout_us % 1000000;
    auto           activity = select(max_sd + 1, &read_sockets, NULL, NULL, &timeout);
    if (activity < 0 && errno != EINTR) {
        lg::error("[nREPL:{}] select error, returned: {}, errno: {}", tcp_port, activity,
                  strerror(errno));
        return std::nullopt;
    }

    // If something happened on the master socket - it's a new connection
    if (FD_ISSET(listening_socket, &read_sockets)) {
        socklen_t addr_len = sizeof(addr);
        auto      new_socket = accept_socket(listening_socket, (sockaddr *)&addr, &addr_len);
        if (new_socket < 0) {
            if (new_socket != -1) {
                lg::error("[nREPL:{}] accept error, returned: {}, errono: {}", tcp_port, new_socket,
                          strerror(errno));
            }
        } else {
            lg::info("[nREPL:{}]: New socket connection: {}:{}:{}", tcp_port,
                     address_to_string(addr), ntohs(addr.sin_port), new_socket);
            // Say hello
            ping_response(new_socket);
            // Track the new socket
            if ((int)client_sockets.size() < max_clients) {
                client_sockets.insert(new_socket);
            } else {
                // Respond with NO and close the socket
                lg::warn("[nREPL:{}]: Maximum clients reached. Rejecting connection.", tcp_port);
                error_response(new_socket, "Maximum clients reached. Rejecting connection.");
                close_socket(new_socket);
            }
        }
    }

    // Check all clients for activity
    for (auto it = client_sockets.begin(); it != client_sockets.end();) {
        int sock = *it;
        if (FD_ISSET(sock, &read_sockets)) {
            // --- Read the fixed-size header first. ---
            //
            // TCP may deliver the header in fragments; loop until we have
            // exactly sizeof(ReplServerHeader) bytes, or the socket dies.
            int  header_got = 0;
            bool header_ok = true;
            while (header_got < (int)header_buffer.size()) {
                auto bytes = read_from_socket(sock, header_buffer.data() + header_got,
                                              header_buffer.size() - header_got);
                if (bytes <= 0) {
                    if (bytes == 0) {
                        lg::warn("[nREPL:{}] Client Disconnected: {}", tcp_port,
                                 address_to_string(addr));
                    } else {
                        lg::warn("[nREPL:{}] Error reading header from {}: {}", tcp_port,
                                 address_to_string(addr), strerror(errno));
                    }
                    close_socket(sock);
                    it = client_sockets.erase(it);
                    header_ok = false;
                    break;
                }
                header_got += bytes;
            }
            if (!header_ok) { continue; }

            auto *header = (ReplServerHeader *)(header_buffer.data());

            // --- PING ---
            // Answer immediately and keep waiting for real messages.
            if (header->type == ReplServerMessageType::PING) {
                ping_response(sock);
                ++it;
                continue;
            }

            // --- EVAL / anything else with a body ---
            // Read the body into the shared `buffer`, resizing it if the
            // client claims a larger payload than we have room for.
            const int expected_size = static_cast<int>(header->length);
            if (expected_size < 0) {
                lg::error("[nREPL:{}] Bad message: negative length {}", tcp_port, expected_size);
                close_socket(sock);
                it = client_sockets.erase(it);
                continue;
            }
            if (expected_size > (int)buffer.size()) {
                // Try to grow the buffer rather than rejecting the message.
                buffer.resize(expected_size);
            }

            int  got = 0;
            bool body_ok = true;
            while (got < expected_size) {
                if (want_exit_callback()) {
                    lg::warn("[nREPL:{}] Terminating nREPL early", tcp_port);
                    return std::nullopt;
                }
                auto bytes = read_from_socket(sock, buffer.data() + got, expected_size - got);
                if (bytes <= 0) {
                    if (bytes == 0) {
                        lg::warn("[nREPL:{}] Client Disconnected mid-message: {}", tcp_port,
                                 address_to_string(addr));
                    } else {
                        lg::warn("[nREPL:{}] Error reading body from {}: {}", tcp_port,
                                 address_to_string(addr), strerror(errno));
                    }
                    close_socket(sock);
                    it = client_sockets.erase(it);
                    body_ok = false;
                    break;
                }
                got += bytes;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (!body_ok) { continue; }

            // --- Deliver the message to the caller. ---
            //
            // We return as soon as one message has been read; the caller is
            // expected to loop. The socket id is included so that callers
            // that want to answer the message (e.g. main.cpp with
            // send_result) can target the correct client.
            ReplServerMessage out;
            out.socket = sock;
            out.payload = std::string(buffer.data(), static_cast<size_t>(expected_size));
            out.type = header->type;

            lg::debug("[nREPL:{}] Received Message: {}", tcp_port, out.payload);
            return out;
        }
        ++it;
    }
    return std::nullopt;
}

void ReplServer::send_result(int socket, const std::string &text) {
    // Compose the wire message: [ReplServerHeader][payload].
    ReplServerHeader header;
    header.length = static_cast<u32>(text.size());
    header.type = ReplServerMessageType::EVAL_RESULT;

    std::vector<char> out;
    out.resize(sizeof(header));
    std::memcpy(out.data(), &header, sizeof(header));
    out.insert(out.end(), text.begin(), text.end());

    auto written = write_to_socket(socket, out.data(), static_cast<int>(out.size()));
    if (written == -1) {
        lg::warn("[nREPL:{}] Failed to send EVAL_RESULT to socket {} — dropping client", tcp_port,
                 socket);
        close_socket(socket);
        client_sockets.erase(socket);
    }
}
