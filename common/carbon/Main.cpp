// carbon/Main.cpp
#include "carbon/Carbon.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"

#include "common/repl/nrepl/ReplServer.h"

#include "util/Config.hpp"
#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "util/ProjectConfig.hpp"

#include "fmt/color.h"
#include "fmt/format.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace carbon;

namespace {

    // ================================================================
    // Runtime argument parsing
    // ================================================================
    //
    // Arguments passed to the called function (carbon <file> <fn>
    // <args...>) are parsed into Variants. This is not part of the
    // tool configuration: it is the payload of the call. It stays
    // here, independent of common::Config.

    Variant parse_argument(const char *arg) {
        char *end = nullptr;

        long long_value = std::strtol(arg, &end, 10);
        if (*end == '\0' && end != arg) return Variant(static_cast<i64>(long_value));

        double dbl_value = std::strtod(arg, &end);
        if (*end == '\0' && end != arg) return Variant(static_cast<f32>(dbl_value));

        if (std::strcmp(arg, "true") == 0 || std::strcmp(arg, "#t") == 0) return Variant(true);
        if (std::strcmp(arg, "false") == 0 || std::strcmp(arg, "#f") == 0) return Variant(false);

        return Variant(std::string(arg));
    }

    // ================================================================
    // --serve wire protocol
    // ================================================================
    //
    // One line per call: (name arg1 arg2 ...)
    //
    // Each argument is one of:
    //   - an integer (optionally with '-' sign)  -> i64
    //   - a float                                 -> f32
    //   - a double-quoted string with \" and \\   -> std::string
    //   - #t / #f / true / false                  -> bool
    //   - anything else                           -> StringId
    //
    // The tokenizer does NOT use script::Reader from sootc: the host
    // must stay independent of the compiler.

    struct ParsedCall {
        std::string          name;
        std::vector<Variant> args;
    };

    class CallParser {
    public:
        explicit CallParser(const std::string &src) : m_src(src), m_pos(0) {}

        ParsedCall parse() {
            skip_ws();
            expect('(');
            skip_ws();

            ParsedCall call;
            call.name = read_symbol_or_string();
            if (call.name.empty()) { fail("expected function name"); }

            while (true) {
                skip_ws();
                if (m_pos >= m_src.size()) { fail("unexpected end of input"); }
                if (m_src[m_pos] == ')') {
                    ++m_pos;
                    break;
                }
                call.args.push_back(read_value());
            }

            skip_ws();
            if (m_pos != m_src.size()) { fail("trailing garbage after ')'"); }
            return call;
        }

    private:
        void fail(const char *what) const {
            throw std::runtime_error(fmt::format("parse error at {}: {}", m_pos, what));
        }

        void skip_ws() {
            while (m_pos < m_src.size() && (m_src[m_pos] == ' ' || m_src[m_pos] == '\t' ||
                                            m_src[m_pos] == '\r' || m_src[m_pos] == '\n')) {
                ++m_pos;
            }
        }

        void expect(char c) {
            if (m_pos >= m_src.size() || m_src[m_pos] != c) {
                fail(fmt::format("expected '{}'", c).c_str());
            }
            ++m_pos;
        }

        std::string read_symbol() {
            const std::size_t start = m_pos;
            while (m_pos < m_src.size()) {
                const char c = m_src[m_pos];
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '(' || c == ')' ||
                    c == '"') {
                    break;
                }
                ++m_pos;
            }
            return m_src.substr(start, m_pos - start);
        }

        std::string read_symbol_or_string() {
            if (m_pos < m_src.size() && m_src[m_pos] == '"') {
                std::string out;
                ++m_pos;
                while (m_pos < m_src.size()) {
                    const char c = m_src[m_pos];
                    if (c == '"') {
                        ++m_pos;
                        return out;
                    }
                    if (c == '\\') {
                        ++m_pos;
                        if (m_pos >= m_src.size()) fail("dangling '\\'");
                        const char e = m_src[m_pos++];
                        if (e == 'n') out += '\n';
                        else if (e == 't')
                            out += '\t';
                        else if (e == 'r')
                            out += '\r';
                        else
                            out += e;
                        continue;
                    }
                    out += c;
                    ++m_pos;
                }
                fail("unterminated string");
            }
            return read_symbol();
        }

        Variant read_value() {
            if (m_src[m_pos] == '"') { return Variant(read_symbol_or_string()); }

            const std::string tok = read_symbol();
            if (tok.empty()) { fail("empty token"); }

            char      *end = nullptr;
            const long lv = std::strtol(tok.c_str(), &end, 10);
            if (*end == '\0' && end != tok.c_str()) { return Variant(static_cast<i64>(lv)); }
            double dv = std::strtod(tok.c_str(), &end);
            if (*end == '\0' && end != tok.c_str()) { return Variant(static_cast<f32>(dv)); }

            if (tok == "true" || tok == "#t") return Variant(true);
            if (tok == "false" || tok == "#f") return Variant(false);

            return Variant(StringId(tok));
        }

        const std::string &m_src;
        std::size_t        m_pos;
    };

    std::string handle_eval(Carbon &carbon, const std::string &payload) {
        try {
            CallParser parser(payload);
            ParsedCall call = parser.parse();

            CallResult r = carbon.call(call.name, call.args);
            if (!r.ok) { return "ERROR: " + r.error; }

            if (r.value.is_null()) { return "null"; }
            return r.value.to_string();
        } catch (const std::exception &e) { return std::string("ERROR: ") + e.what(); }
    }

    // ================================================================
    // --serve loop
    // ================================================================
    //
    // Single thread: the socket and the tick loop share the same
    // thread, and the socket wait is bounded by the time left until
    // the next frame. At most one message per frame is served, so
    // incoming traffic cannot starve the tick loop.
    //
    // The loop does NOT exit when the scheduler is empty: the host
    // must stay alive between calls, waiting for the first spawn.

    void serve_loop(Carbon &carbon, u32 hz, int port) {
        ReplServer server([] { return false; }, port);

        if (!server.init_server()) {
            fmt::print(stderr, "carbon: failed to start server on port {}\n", port);
            return;
        }

        using clock = std::chrono::steady_clock;
        const auto period = std::chrono::microseconds(1'000'000 / (hz ? hz : 60));
        auto       next = clock::now();

        while (!carbon.should_exit()) {
            const auto left =
                std::chrono::duration_cast<std::chrono::microseconds>(next - clock::now());
            const int timeout_us = static_cast<int>(std::max<i64>(0, left.count()));

            if (auto msg = server.get_msg(timeout_us)) {
                const std::string reply = handle_eval(carbon, msg->payload);
                server.send_result(msg->socket, reply);
            }

            if (clock::now() >= next) {
                carbon.tick();
                next += period;
                if (clock::now() > next) next = clock::now();
            }
        }
    }

} // namespace

namespace {

    // ================================================================
    // Early scan
    // ================================================================
    //
    // --config, --no-config and --project-root decide where (and
    // whether) soot.json is read, so they must be known before
    // load_json(). Everything else goes through apply_cli().

    struct EarlyArgs {
        std::string project_root;
        std::string config_file;
        bool        no_config = false;
    };

    EarlyArgs scan_early_args(int argc, char *argv[]) {
        EarlyArgs out;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                out.config_file = argv[++i];
            } else if (arg == "--project-root" || arg == "--project") {
                if (i + 1 < argc) out.project_root = argv[++i];
            } else if (arg == "--no-config") {
                out.no_config = true;
            }
        }
        return out;
    }

    // ================================================================
    // Logging
    // ================================================================

    void setup_logging(const common::Config &cfg) {
        lg::set_console_level(cfg.common.log_level);
        if (!cfg.common.log_file.empty()) {
            lg::set_file(cfg.common.log_file.string());
            lg::set_file_level(cfg.common.log_level);
            lg::info("Log file enabled: {}", cfg.common.log_file.string());
        }
        lg::initialize();
    }

    // ================================================================
    // CarbonConfig from Config
    // ================================================================
    //
    // CarbonConfig is the runtime's DTO for initialize(); it is not
    // the same thing as common::Config. This is the one place that
    // bridges them, same as make_compilation_options in sootc.

    CarbonConfig make_carbon_config(const common::Config &cfg) {
        CarbonConfig out;
        out.sidbase_path = cfg.common.sidbase.string();
        out.search_paths.clear();
        for (const auto &p : cfg.common.search_paths) { out.search_paths.push_back(p); }
        return out;
    }

    // ================================================================
    // listen parsing
    // ================================================================
    //
    // HostConfig::listen is "host:port" or just "port". Empty means
    // 127.0.0.1:8181.

    struct ListenAddress {
        std::string host = "127.0.0.1";
        int         port = 8181;
    };

    ListenAddress parse_listen(const std::string &s) {
        ListenAddress out;
        if (s.empty()) return out;

        const auto colon = s.rfind(':');
        if (colon == std::string::npos) {
            // "port" only
            try {
                out.port = std::stoi(s);
            } catch (...) { /* keep default */
            }
            return out;
        }
        out.host = s.substr(0, colon);
        try {
            out.port = std::stoi(s.substr(colon + 1));
        } catch (...) { /* keep default */
        }
        return out;
    }

} // namespace

int main(int argc, char *argv[]) {
    // ----- Layers -----
    //
    // Same order as sootc: early scan, JSON, env, CLI.
    // apply_cli is called with Scope::Host, so sootc-only options
    // (--mode, --dump, ...) are rejected with a precise message
    // rather than "unknown option".

    const EarlyArgs early = scan_early_args(argc, argv);

    if (early.no_config && !early.config_file.empty()) {
        fmt::print(stderr, "error: --config and --no-config are mutually exclusive\n");
        return 2;
    }

    common::Config cfg;

    if (!early.no_config) {
        if (auto err = common::load_json(early.config_file, early.project_root, cfg)) {
            fmt::print(stderr, "error: {}\n", *err);
            return 2;
        }
    }

    if (auto err = common::apply_env(cfg)) {
        fmt::print(stderr, "error: {}\n", *err);
        return 2;
    }

    std::vector<std::string> inputs;
    bool                     no_config = early.no_config;
    if (auto err = common::apply_cli(argc, argv, cfg, common::Scope::Host, inputs, no_config)) {
        fmt::print(stderr, "error: {}\n", *err);
        return 2;
    }

    setup_logging(cfg);

    // ----- Project root -----

    if (cfg.common.project_root.empty()) {
        cfg.common.project_root = std::filesystem::current_path();
    } else if (cfg.common.project_root.is_relative()) {
        cfg.common.project_root = std::filesystem::absolute(cfg.common.project_root);
    }
    common::set_project_path(cfg.common.project_root);
    lg::info("PROJECT = {}", cfg.common.project_root.string());

    // ----- Carbon up -----

    CarbonConfig carbon_cfg = make_carbon_config(cfg);

    Carbon carbon;
    if (!carbon.initialize(carbon_cfg)) {
        fmt::print(stderr, "carbon: failed to initialize runtime\n");
        return 1;
    }

    // ----- Load modules (-l / --load) -----

    for (const auto &path : cfg.host.load_files) {
        if (!carbon.load_module_with_imports(path, carbon_cfg.search_paths)) {
            fmt::print(stderr, "carbon: failed to load '{}'\n", path.string());
            carbon.shutdown();
            return 1;
        }
    }

    // ----- Positional .bin (if any) -----

    std::size_t pos_start = 0;
    if (!inputs.empty() && std::filesystem::exists(inputs[0])) {
        if (!carbon.load_module_with_imports(inputs[0], carbon_cfg.search_paths)) {
            fmt::print(stderr, "carbon: failed to load '{}'\n", inputs[0]);
            carbon.shutdown();
            return 1;
        }
        pos_start = 1;
    }

    // ----- --serve -----

    if (cfg.host.serve) {
        const ListenAddress addr = parse_listen(cfg.host.listen);
        serve_loop(carbon, static_cast<u32>(cfg.host.hz), addr.port);
        carbon.shutdown();
        return 0;
    }

    // ----- --run NAME -----

    if (!cfg.host.run_state_script.empty()) {
        const std::string &name = cfg.host.run_state_script;

        u64 pid = carbon.spawn_process(name, {});
        if (pid == 0) {
            fmt::print(stderr, "carbon: failed to spawn '{}'\n", name);
            carbon.shutdown();
            return 1;
        }

        fmt::print(fg(fmt::color::cyan), "carbon: spawned '{}' (pid={}, {} Hz)\n", name, pid,
                   cfg.host.hz);

        carbon::TickConfig tick_cfg;
        tick_cfg.hz = static_cast<u32>(cfg.host.hz);
        tick_cfg.max_ticks = cfg.host.ticks;
        tick_cfg.realtime = true;
        tick_cfg.until_empty = true;

        const u64 ticks = carbon.run_loop(tick_cfg);
        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                   "carbon: tick loop finished after {} ticks\n", ticks);

        carbon.shutdown();
        return 0;
    }

    // ----- Default: call one function once -----

    if (inputs.size() <= pos_start) {
        fmt::print(stderr, "carbon: no function name given\n");
        common::print_help(common::Scope::Host, "carbon");
        carbon.shutdown();
        return 1;
    }

    const std::string    fn_name = inputs[pos_start];
    std::vector<Variant> args;
    for (std::size_t i = pos_start + 1; i < inputs.size(); ++i) {
        args.push_back(parse_argument(inputs[i].c_str()));
    }

    fmt::print(fg(fmt::color::cyan), "carbon: calling {}({} args)\n", fn_name, args.size());

    CallResult result = carbon.call(fn_name, args);
    if (!result.ok) {
        fmt::print(stderr, "carbon: {}\n", result.error);
        carbon.shutdown();
        return 1;
    }

    fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "{} = {}\n", fn_name,
               result.value.to_string());

    carbon.shutdown();
    return 0;
}