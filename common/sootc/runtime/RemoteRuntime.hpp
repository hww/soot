// sootc/runtime/RemoteRuntime.hpp
#pragma once

#include "sootc/runtime/Runtime.hpp"

#include <memory>
#include <string>

namespace sootc {

    /// @brief A Runtime that sends work to an external host.
    ///
    /// @details The host is a separate process (carbon.exe) that owns
    ///          the full runtime: processes, entities, game state.
    ///          Communication goes over the existing cross-socket
    ///          protocol used by ReplServer/ReplClient — we do NOT
    ///          invent a new protocol here.
    ///
    ///          The wire format is the same ReplServerMessageType
    ///          / ReplServerHeader format that ReplServer already
    ///          understands:
    ///
    ///            struct ReplServerHeader {
    ///              u32 length;   // length of the payload that follows
    ///              u32 type;     // one of ReplServerMessageType
    ///            };
    ///
    ///          For now, only EVAL messages are used: the payload is a
    ///          single form encoded as SOOT text (the same encoding the
    ///          REPL uses). load_module() sends the binary as a
    ///          base64-encoded EVAL of a special (load-binary "...")
    ///          form; the exact shape can be adjusted once carbon.exe
    ///          implements it.
    ///
    ///          This class is intentionally a stub: it connects, it
    ///          knows how to send and receive frames, but the actual
    ///          handshake and semantics are left for when carbon.exe
    ///          is ready. Until then, all methods return a clear
    ///          "not implemented" error rather than silently
    ///          succeeding, so that a misconfigured `--runtime <url>`
    ///          fails loudly.
    class RemoteRuntime : public Runtime {
        std::string m_url; // e.g. "127.0.0.1:8181"
        std::string m_host;
        int         m_port = -1;
        bool        m_connected = false;

    public:
        explicit RemoteRuntime(std::string url);

        bool load_module(carbon::BinaryFile &&file) override;

        RuntimeResult call(const std::string                  &name,
                           const std::vector<carbon::Variant> &args) override;

        std::string name() const override { return fmt::format("remote:{}", m_url); }
    };

} // namespace sootc