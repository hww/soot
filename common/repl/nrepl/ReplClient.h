#pragma once

#include "ReplServer.h"

#include "cross_sockets/XSocketClient.hpp"

#include <optional>
#include <string>

class ReplClient : public XSocketClient {
public:
    using XSocketClient::XSocketClient;
    virtual ~ReplClient() = default;

    ReplClient &operator=(const ReplClient &) { return *this; }

    // Send a form to the server for evaluation and wait for its result.
    //
    // Returns:
    //   * std::nullopt                       — not connected, socket
    //                                          read/write failed, or
    //                                          the server sent an
    //                                          unexpected message type.
    //   * a string starting with "ERROR: "   — the server reported an
    //                                          error while evaluating
    //                                          the form.
    //   * any other string                   — the printed result of
    //                                          the form.
    //
    // Callers that do not need the result can use eval_no_response()
    // to avoid blocking.
    std::optional<std::string> eval(const std::string &form);

    // Fire-and-forget variant: send the form, do not wait for a reply.
    // Kept for backwards compatibility with code that only wants to
    // push a form and does not care about the result.
    void eval_no_response(const std::string &form);
};