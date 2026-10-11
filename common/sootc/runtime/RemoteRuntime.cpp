#include "sootc/runtime/RemoteRuntime.hpp"

#include "common/util/Log.hpp"

#include <utility>

namespace sootc {

    // Parse "host:port" or "scheme://host:port". The scheme prefix, if
    // present, is stripped. Everything after the first ':' is the port.
    static std::pair<std::string, int> parse_url(const std::string &url) {
        std::string s = url;

        const auto scheme_end = s.find("://");
        if (scheme_end != std::string::npos) { s = s.substr(scheme_end + 3); }

        const auto colon = s.find(':');
        if (colon == std::string::npos) { return {s, 8181}; }
        return {s.substr(0, colon), std::stoi(s.substr(colon + 1))};
    }

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

    RuntimeResult RemoteRuntime::call(const std::string                  &name,
                                      const std::vector<carbon::Variant> &args) {
        RuntimeResult out;

        if (!m_client || !m_client->is_connected()) {
            out.ok = false;
            out.error = "RemoteRuntime: not connected";
            return out;
        }

        // Build "(name arg1 arg2 ...)".
        std::string form = "(" + name;
        for (const auto &arg : args) {
            form += " ";
            form += arg.to_string();
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