#pragma once

#include <optional>
#include <set>
#include <string>

#include "cross_sockets/XSocketServer.hpp"

enum ReplServerMessageType {
    PING = 0,
    EVAL = 10,
    SHUTDOWN = 20,

    // Response sent by the server back to the client after an EVAL.
    //
    // The payload is a single line of text: either the printed result
    // of the form, or an error message prefixed with "ERROR: ".
    //
    // Client libraries that only send forms (e.g. the old ReplClient)
    // can ignore this message type; clients that need the result of
    // an EVAL (e.g. soot's RemoteRuntime) must wait for it.
    EVAL_RESULT = 11,
};

struct ReplServerHeader {
    u32 length;
    u32 type;
};

/// @brief One inbound message from a REPL client.
///
/// @details get_msg used to return just the payload string, which was
///          enough while the server was only a fire-and-forget
///          printer. Now that the server can also answer an EVAL with
///          an EVAL_RESULT, the caller must know which socket sent
///          the message, so that send_result can be directed at the
///          right client. Hence the extra fields.
struct ReplServerMessage {
    int         socket = -1; ///< client that sent the message
    std::string payload;     ///< raw message text
    u32         type = 0;    ///< ReplServerMessageType
};

class ReplServer : public XSocketServer {
public:
    using XSocketServer::XSocketServer;
    virtual ~ReplServer();

    void post_init() override;

    /// @brief Wait for and read one inbound message.
    /// @details Returns std::nullopt when no message is ready within the
    ///          polling timeout, when the server is shutting down, or
    ///          when the connection was lost. The caller is expected to
    ///          loop on this method.
    std::optional<ReplServerMessage> get_msg(int timeout_us = 100000);

    /// @brief Send an EVAL_RESULT message back to a specific client.
    ///
    /// @details The payload is a single block of text — the captured
    ///          output of evaluating the client's form. If the socket is
    ///          invalid or the write fails, the client is dropped and
    ///          the connection closed; this matches the behaviour of
    ///          error_response and ping_response.
    ///
    /// @param socket  Client socket, as returned in ReplServerMessage.
    /// @param text    Result text (may contain newlines).
    void send_result(int socket, const std::string &text);

private:
    int               max_clients = 50;
    std::vector<char> header_buffer = std::vector<char>((int)sizeof(ReplServerHeader));
    fd_set            read_sockets;
    std::set<int>     client_sockets = {};

    void error_response(int socket, const std::string &error);
    void ping_response(int socket);

};