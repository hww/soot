// carbon/Main.cpp
#include "carbon/Carbon.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"

#include "common/util/Log.hpp"
#include "fmt/color.h"
#include "fmt/format.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
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
        fmt::print("  -h, --help              Show this help\n");
        fmt::print("\nNotes:\n");
        fmt::print("  Imports are resolved via .dci manifests next to each .bin.\n");
        fmt::print("  Use -I to add directories where .bin/.dci pairs live.\n");
    }

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

} // namespace

int main(int argc, char *argv[]) {
    std::vector<std::string>           load_files;
    std::vector<std::filesystem::path> search_paths;
    std::vector<std::string>           positional;
    std::string                        sidbase_path;
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
        if (!arg.empty() && arg[0] == '-') {
            fmt::print(stderr, "carbon: unknown option '{}'\n", arg);
            print_usage(argv[0]);
            return 1;
        }
        positional.push_back(arg);
    }

    if (want_help || (load_files.empty() && positional.size() < 2)) {
        print_usage(argv[0]);
        return want_help ? 0 : 1;
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

    // ----- Call -----
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