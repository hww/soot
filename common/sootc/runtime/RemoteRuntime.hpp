#pragma once

#include "sootc/runtime/Runtime.hpp"

#include "common/repl/nrepl/ReplClient.h"

#include <memory>
#include <string>

namespace sootc {

    /// @brief A Runtime that sends work to an external host over the
    ///        cross-socket nREPL protocol.
    ///
    /// @details The host is a separate process (carbon.exe) that owns
    ///          the full runtime: processes, entities, game state.
    ///          Communication goes over the same socket protocol used
    ///          by ReplServer / ReplClient:
    ///
    ///            request:  [header: EVAL][payload: form text]
    ///            response: [header: EVAL_RESULT][payload: result text]
    ///
    ///          load_module is intentionally a no-op: the host is
    ///          assumed to manage its own module loading (it is the
    ///          one that actually compiles and registers game code).
    ///
    ///          call() builds the SOOT expression "(name arg1 arg2 ...)"
    ///          from the function name and the Variant arguments, sends
    ///          it, and waits for the server's reply. A reply starting
    ///          with "ERROR: " is treated as a runtime error; anything
    ///          else is the printed result of the form.
    ///
    ///          String arguments are sent double-quoted and escaped
    ///          (\", \\); everything else uses Variant::to_string().
    ///          Without quoting, (spawn my gate) and (spawn "my gate")
    ///          would be indistinguishable on the wire.
    class RemoteRuntime : public Runtime {
        std::string                 m_url;
        std::string                 m_host;
        int                         m_port = -1;
        std::unique_ptr<ReplClient> m_client;

    public:
        explicit RemoteRuntime(std::string url);

        bool load_module(carbon::BinaryFile &&file) override;

        bool run_top_level(const std::string &logical_name) override;

        RuntimeResult call(const std::string                  &name,
                           const std::vector<carbon::Variant> &args) override;

        std::string name() const override { return fmt::format("remote:{}", m_url); }

        bool has_symbol(const std::string &) const override { return true; }
    };

} // namespace sootc