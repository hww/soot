#include "sootc/runtime/RemoteRuntime.hpp"

#include "common/util/Log.hpp"

#include <utility>
#include <Carbon.hpp>

namespace sootc {

    namespace {

        // Parse "host:port" or "scheme://host:port". The scheme prefix,
        // if present, is stripped. Everything after the first ':' is
        // the port.
        std::pair<std::string, int> parse_url(const std::string &url) {
            std::string s = url;

            const auto scheme_end = s.find("://");
            if (scheme_end != std::string::npos) { s = s.substr(scheme_end + 3); }

            const auto colon = s.find(':');
            if (colon == std::string::npos) { return {s, 8181}; }
            return {s.substr(0, colon), std::stoi(s.substr(colon + 1))};
        }

        // Format a single Variant as a token for the wire protocol.
        //
        //   String  -> "..." with \" and \\ escaped
        //   Pointer -> null (the host cannot resolve a client address)
        //   Bool    -> true / false
        //   Int     -> decimal
        //   Float   -> decimal (fmt default)
        //   Null    -> null
        //
        // The host's CallParser reads exactly this grammar:
        //   - a token starting with '"' is a string (with escapes)
        //   - an integer token is i64
        //   - a float token is f32
        //   - true / #t, false / #f are booleans
        //   - anything else is a StringId (symbol)
        //
        // StringId arguments (e.g. event names) are NOT quoted; they
        // arrive at the host as symbols and are converted to StringId
        // by CallParser::read_value.
        std::string quote_if_string(const carbon::Variant &v) {
            switch (v.get_type()) {
            case carbon::RuntimeType::Pointer: return "null";
            case carbon::RuntimeType::Null: return "null";
            default: return v.to_string();
            }
        }

    } // namespace

    RemoteRuntime::RemoteRuntime(std::string url) : m_url(std::move(url)) {
        auto [host, port] = parse_url(m_url);
        m_host = std::move(host);
        m_port = port;

        m_client = std::make_unique<ReplClient>(m_port);

        if (m_client->connect()) {
            lg::info("RemoteRuntime: connected to {}:{}", m_host, m_port);
        } else {
            lg::warn("RemoteRuntime: could not connect to {}:{} (calls will fail)", m_host, m_port);
            m_client.reset();
        }
    }

    bool RemoteRuntime::load_module(carbon::BinaryFile &&file) {
        // The host compiles and registers modules on its own side.
        // Sending the binary over the wire is a future extension; for
        // now we simply acknowledge the call so that compile_and_report
        // does not warn.
        (void)file;
        return true;
    }

    CallResult RemoteRuntime::run_top_level(const std::string &logical_name) {
        // The host runs its own top-level when it loads modules. Sootc
        // does not send binaries over the wire, so there is nothing
        // for us to run here.
        (void)logical_name;
        return carbon::CallResult{};
    }

    CallResult RemoteRuntime::call(const std::string                  &name,
                                      const std::vector<carbon::Variant> &args) {
        CallResult out;

        if (!m_client || !m_client->is_connected()) {
            out.ok = false;
            out.error = "RemoteRuntime: not connected";
            return out;
        }

        // Build "(name arg1 arg2 ...)".
        //
        // Every argument goes through quote_if_string so that strings
        // keep their boundaries. Integer and float arguments are
        // printed by Variant::to_string() in a form the host's
        // CallParser accepts.
        std::string form = "(" + name;
        for (const auto &arg : args) {
            form += " ";
            form += quote_if_string(arg);
        }
        form += ")";

        auto reply = m_client->eval(form);
        if (!reply) {
            out.ok = false;
            out.error = "RemoteRuntime: eval failed (no response or bad frame)";
            return out;
        }

        // The server prefixes errors with "ERROR: ".
        if (reply->rfind("ERROR: ", 0) == 0) {
            out.ok = false;
            out.error = reply->substr(7);
            return out;
        }

        out.ok = true;
        out.value = carbon::Variant(*reply);
        return out;
    }

} // namespace sootc