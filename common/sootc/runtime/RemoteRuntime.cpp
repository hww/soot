// sootc/runtime/RemoteRuntime.cpp
#include "sootc/runtime/RemoteRuntime.hpp"

#include "util/Log.hpp"

namespace sootc {

    // Parse "host:port" or "http://host:port". The scheme prefix, if
    // present, is stripped. Everything after the first ':' is treated
    // as the port.
    static std::pair<std::string, int> parse_url(const std::string &url) {
        std::string s = url;

        // Strip a scheme prefix like "http://" or "tcp://".
        const auto scheme_end = s.find("://");
        if (scheme_end != std::string::npos) { s = s.substr(scheme_end + 3); }

        const auto colon = s.find(':');
        if (colon == std::string::npos) {
            return {s, 8181}; // default port
        }
        return {s.substr(0, colon), std::stoi(s.substr(colon + 1))};
    }

    RemoteRuntime::RemoteRuntime(std::string url) : m_url(std::move(url)) {
        auto [host, port] = parse_url(m_url);
        m_host = std::move(host);
        m_port = port;

        lg::info("RemoteRuntime: target {}:{} (protocol not yet implemented)", m_host, m_port);
        // TODO: use ReplClient to connect to the host.
        //       ReplClient client(m_port);
        //       if (!client.connect()) { ... }
        //       m_connected = true;
    }

    bool RemoteRuntime::load_module(carbon::BinaryFile &&file) {
        (void)file;
        // TODO: serialize the BinaryFile and send it as an EVAL
        //       message with a special (load-binary "...") form.
        //       Reuse ReplClient::eval() once the protocol is
        //       finalized.
        return false;
    }

    RuntimeResult RemoteRuntime::call(const std::string                  &name,
                                      const std::vector<carbon::Variant> &args) {
        (void)name;
        (void)args;
        RuntimeResult out;
        out.ok = false;
        out.error = "RemoteRuntime: protocol not yet implemented";
        return out;
    }

} // namespace sootc