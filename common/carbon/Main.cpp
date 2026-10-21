// carbon/Main.cpp
#include "carbon/Carbon.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"

#include "common/repl/nrepl/ReplServer.h"

#include "common/util/Log.hpp"

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

    void print_usage(const char *prog) {
        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "carbon — Carbon VM runtime\n\n");
        fmt::print("Usage:\n");
        fmt::print("  {} [options] <file.bin> <function> [args...]\n", prog);
        fmt::print("\nOptions:\n");
        fmt::print("  -I, --include <dir>     Add a module search path (repeatable)\n");
        fmt::print("  -l, --load <file.bin>   Load a module explicitly (repeatable)\n");
        fmt::print("  -s, --sidbase <file>    Load a sidbase.bin\n");
        fmt::print("  --run <name>            Spawn a state-script and run the tick loop\n");
        fmt::print("  --hz <n>                Tick rate in Hz (default: 60)\n");
        fmt::print("  --ticks <n>             Stop after N ticks (default: unlimited)\n");
        fmt::print("  --serve                 Run the HTTP/REPL host (default port 8181)\n");
        fmt::print("  --port <n>              Port for --serve (default: 8181)\n");
        fmt::print("  -h, --help              Show this help\n");
        fmt::print("\nNotes:\n");
        fmt::print("  Imports are resolved via .dci manifests next to each .bin.\n");
        fmt::print("  Use -I to add directories where .bin/.dci pairs live.\n");
        fmt::print("  In --serve mode the host ticks itself; the client only sends\n");
        fmt::print("  function calls. Time is discrete: one tick is one frame.\n");
    }

    // Parse a single CLI argument into a Variant.
    //   - integer (decimal)   -> i64
    //   - float               -> f32
    //   - true / #t           -> bool true
    //   - false / #f          -> bool false
    //   - anything else       -> std::string (Variant::String)
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

    // ----------------------------------------------------------------
    // Tiny S-expression tokenizer for the --serve protocol.
    //
    // The wire format is one line per call:
    //
    //     (name arg1 arg2 ...)
    //
    // where each argument is one of:
    //
    //     - an integer (optionally with '-' sign)  -> i64
    //     - a float                                 -> f32
    //     - a double-quoted string with \" and \\   -> std::string
    //     - #t / #f / true / false                  -> bool
    //     - anything else                           -> StringId
    //
    // The tokenizer does NOT use script::Reader from sootc: the host
    // must stay independent of the compiler, and the protocol is small
    // and stable enough that a 40-line parser is cheaper than a
    // dependency.
    // ----------------------------------------------------------------

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

            // Integer or float.
            char           *end = nullptr;
            const long long lv = std::strtol(tok.c_str(), &end, 10);
            if (*end == '\0' && end != tok.c_str()) { return Variant(static_cast<i64>(lv)); }
            double dv = std::strtod(tok.c_str(), &end);
            if (*end == '\0' && end != tok.c_str()) { return Variant(static_cast<f32>(dv)); }

            if (tok == "true" || tok == "#t") return Variant(true);
            if (tok == "false" || tok == "#f") return Variant(false);

            // Everything else is treated as a StringId (symbols,
            // identifiers, event names).
            return Variant(StringId(tok));
        }

        const std::string &m_src;
        std::size_t        m_pos;
    };

    // Handle one wire payload: parse it, call the function, build the
    // reply string. The reply is either the function's result (as text)
    // or "ERROR: <message>" if anything went wrong.
    std::string handle_eval(Carbon &carbon, const std::string &payload) {
        try {
            CallParser parser(payload);
            ParsedCall call = parser.parse();

            CallResult r = carbon.call(call.name, call.args);
            if (!r.ok) { return "ERROR: " + r.error; }

            // A null result is not an error, but the client should see
            // that the call happened. Return the string "null" for it.
            if (r.value.is_null()) { return "null"; }
            return r.value.to_string();
        } catch (const std::exception &e) { return std::string("ERROR: ") + e.what(); }
    }

    // ----------------------------------------------------------------
    // --serve: the host loop.
    //
    // Single thread: the socket and the tick loop share the same
    // thread, and the socket wait is bounded by the time left until
    // the next frame. At most one message per frame is served, so
    // incoming traffic cannot starve the tick loop.
    //
    // The loop does NOT exit when the scheduler is empty: the host
    // must stay alive between calls, waiting for the first spawn.
    // ----------------------------------------------------------------
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

int main(int argc, char *argv[]) {
    std::vector<std::string>           load_files;
    std::vector<std::filesystem::path> search_paths;
    std::vector<std::string>           positional;
    std::string                        sidbase_path;
    std::string                        run_state_script; // --run <name>
    u32                                hz = 60;          // --hz <n>
    i64                                max_ticks = -1;   // --ticks <n>
    bool                               serve = false;    // --serve
    int                                port = 8181;      // --port <n>
    bool                               want_help = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            want_help = true;
            break;
        }
        if (arg == "-I" || arg == "--include") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: '{}' requires a dir\n", arg);
                return 1;
            }
            search_paths.emplace_back(argv[++i]);
            continue;
        }
        if (arg == "-l" || arg == "--load") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: '{}' requires a file\n", arg);
                return 1;
            }
            load_files.emplace_back(argv[++i]);
            continue;
        }
        if (arg == "-s" || arg == "--sidbase") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: '{}' requires a file\n", arg);
                return 1;
            }
            sidbase_path = argv[++i];
            continue;
        }
        if (arg == "--run") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: '{}' requires a state-script name\n", arg);
                return 1;
            }
            run_state_script = argv[++i];
            continue;
        }
        if (arg == "--hz") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: '{}' requires a number\n", arg);
                return 1;
            }
            char      *end = nullptr;
            const long v = std::strtol(argv[++i], &end, 10);
            if (*end != '\0' || v <= 0 || v > 1000) {
                fmt::print(stderr, "carbon: --hz must be 1..1000\n");
                return 1;
            }
            hz = static_cast<u32>(v);
            continue;
        }
        if (arg == "--ticks") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: '{}' requires a number\n", arg);
                return 1;
            }
            char           *end = nullptr;
            const long long v = std::strtoll(argv[++i], &end, 10);
            if (*end != '\0' || v < -1) {
                fmt::print(stderr, "carbon: --ticks must be >= -1\n");
                return 1;
            }
            max_ticks = static_cast<i64>(v);
            continue;
        }
        if (arg == "--serve") {
            serve = true;
            continue;
        }
        if (arg == "--port") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: '{}' requires a number\n", arg);
                return 1;
            }
            char      *end = nullptr;
            const long v = std::strtol(argv[++i], &end, 10);
            if (*end != '\0' || v < 1 || v > 65535) {
                fmt::print(stderr, "carbon: --port must be 1..65535\n");
                return 1;
            }
            port = static_cast<int>(v);
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            fmt::print(stderr, "carbon: unknown option '{}'\n", arg);
            print_usage(argv[0]);
            return 1;
        }
        positional.push_back(arg);
    }

    // --run and --serve are mutually exclusive with the positional
    // "call one function" form. --run may still take a positional
    // file, which is loaded before spawn.
    const bool has_action = !run_state_script.empty() || serve;
    if (want_help) {
        print_usage(argv[0]);
        return 0;
    }
    if (!has_action && load_files.empty() && positional.size() < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // ----- Carbon up -----
    CarbonConfig config;
    config.sidbase_path = sidbase_path;

    Carbon carbon;
    if (!carbon.initialize(config)) {
        fmt::print(stderr, "carbon: failed to initialize runtime\n");
        return 1;
    }

    // ----- Load modules -----
    for (const auto &path : load_files) {
        if (!carbon.load_module_with_imports(path, search_paths)) {
            fmt::print(stderr, "carbon: failed to load '{}'\n", path);
            carbon.shutdown();
            return 1;
        }
    }

    std::size_t pos_start = 0;
    if (!positional.empty() && std::filesystem::exists(positional[0])) {
        if (!carbon.load_module_with_imports(positional[0], search_paths)) {
            fmt::print(stderr, "carbon: failed to load '{}'\n", positional[0]);
            carbon.shutdown();
            return 1;
        }
        pos_start = 1;
    }

    // ----- --serve: host loop -----
    //
    // The host ticks itself; the client sends function calls over the
    // wire. The positional "call" form is not used in this mode.
    if (serve) {
        if (port != 8181) {
            // ReplServer port is currently fixed by REPL::Config;
            // a custom --port would require ReplServer to accept one.
            fmt::print(stderr, "carbon: --port {} ignored (ReplServer uses the configured port)\n",
                       port);
        }
        serve_loop(carbon, hz, port);
        carbon.shutdown();
        return 0;
    }

    // ----- --run: spawn + tick loop -----
    //
    // Spawns the named state-script and advances the scheduler at the
    // requested rate. The loop exits when the scheduler empties, the
    // tick budget is exhausted, or the runtime is asked to exit.
    if (!run_state_script.empty()) {
        u64 pid = carbon.spawn_process(run_state_script, {});
        if (pid == 0) {
            fmt::print(stderr, "carbon: failed to spawn '{}'\n", run_state_script);
            carbon.shutdown();
            return 1;
        }

        fmt::print(fg(fmt::color::cyan), "carbon: spawned '{}' (pid={}, {} Hz)\n", run_state_script,
                   pid, hz);

        carbon::TickConfig cfg;
        cfg.hz = hz;
        cfg.max_ticks = max_ticks;
        cfg.realtime = true;
        cfg.until_empty = true;

        const u64 ticks = carbon.run_loop(cfg);
        fmt::print(fg(fmt::color::green) | fmt::emphasis::bold,
                   "carbon: tick loop finished after {} ticks\n", ticks);

        carbon.shutdown();
        return 0;
    }

    // ----- Default: call one function once -----
    if (positional.size() <= pos_start) {
        fmt::print(stderr, "carbon: no function name given\n");
        print_usage(argv[0]);
        carbon.shutdown();
        return 1;
    }

    const std::string    fn_name = positional[pos_start];
    std::vector<Variant> args;
    for (std::size_t i = pos_start + 1; i < positional.size(); ++i) {
        args.push_back(parse_argument(positional[i].c_str()));
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