// carbon/Main.cpp
#include "carbon/file/BinaryFile.hpp"
#include "carbon/file/DCIFile.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/kernel/Kernel.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/VirtualMachine.hpp"

#include "common/util/Log.hpp"
#include "fmt/color.h"
#include "fmt/format.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

using namespace carbon;

namespace {

    void print_usage(const char *prog) {
        fmt::print(fg(fmt::color::cyan) | fmt::emphasis::bold, "carbon — Carbon VM runtime\n\n");
        fmt::print("Usage:\n");
        fmt::print("  {} [options] <file.bin> <function> [args...]\n", prog);
        fmt::print("  {} [options] -I <dir> -I <dir> <file.bin> <function> [args...]\n", prog);
        fmt::print("\nOptions:\n");
        fmt::print("  -I, --include <dir>     Add a module search path (repeatable)\n");
        fmt::print("  -l, --load <file.bin>   Load a module explicitly (repeatable)\n");
        fmt::print("  -h, --help              Show this help\n");
        fmt::print("\nNotes:\n");
        fmt::print("  Every module is expected to declare its dependencies in a\n");
        fmt::print("  .dci manifest next to its .bin. The loader reads the\n");
        fmt::print("  manifest and recursively loads every import it lists.\n");
        fmt::print("  Use -I to add directories where .bin/.dci pairs can be\n");
        fmt::print("  found. If no -I is given, only the directory of the\n");
        fmt::print("  module itself is searched.\n");
        fmt::print("\nExamples:\n");
        fmt::print("  {} -I build build/mkvec.bin mkvec\n", prog);
        fmt::print("  {} -I build -I modules build/main.bin main-fn 1 2 3\n", prog);
    }

    Variant parse_argument(const char *arg) {
        char *end = nullptr;

        long long_value = std::strtol(arg, &end, 10);
        if (*end == '\0' && end != arg) { return Variant(static_cast<i64>(long_value)); }

        double dbl_value = std::strtod(arg, &end);
        if (*end == '\0' && end != arg) { return Variant(static_cast<f32>(dbl_value)); }

        if (std::strcmp(arg, "true") == 0 || std::strcmp(arg, "#t") == 0) { return Variant(true); }
        if (std::strcmp(arg, "false") == 0 || std::strcmp(arg, "#f") == 0) {
            return Variant(false);
        }

        return Variant(std::string(arg));
    }

} // namespace

namespace {

    // Recursive module loader.
    //
    // bin_path:       the .bin to load
    // already_loaded: dedup set for cyclic imports
    // search_paths:   directories to search for imported modules
    //
    // Steps:
    //   1. If the module was already loaded, return.
    //   2. Read the module's .dci, if it exists.
    //   3. Recursively load every import listed in the .dci.
    //   4. Load the .bin into Globals.
    bool load_module_recursive(const std::filesystem::path              &bin_path,
                               std::set<std::filesystem::path>          &already_loaded,
                               const std::vector<std::filesystem::path> &search_paths) {

        namespace fs = std::filesystem;
        std::error_code ec;

        fs::path canonical = fs::weakly_canonical(bin_path, ec);
        if (ec) { canonical = bin_path; }

        if (already_loaded.count(canonical)) { return true; }
        already_loaded.insert(canonical);

        // Read the .dci manifest if it exists.
        fs::path dci_path = bin_path;
        dci_path.replace_extension(".dci");

        if (fs::exists(dci_path)) {
            try {
                carbon::DCIFile manifest = carbon::DCIFile::parse(dci_path.string());
                fs::path        parent_dir = bin_path.parent_path();

                for (const auto &imp : manifest.imports) {
                    const std::string dep_name = imp.to_string();

                    // Search order:
                    //   1. next to the importing module
                    //   2. in each -I directory
                    std::vector<fs::path> candidates;
                    candidates.push_back(parent_dir / (dep_name + ".bin"));
                    for (const auto &root : search_paths) {
                        candidates.push_back(root / (dep_name + ".bin"));
                    }

                    bool found = false;
                    for (const auto &candidate : candidates) {
                        if (fs::exists(candidate)) {
                            if (!load_module_recursive(candidate, already_loaded, search_paths)) {
                                return false;
                            }
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        fmt::print(stderr,
                                   "carbon: error: module '{}' imports '{}' but no "
                                   "{} file was found (searched {} locations)\n",
                                   bin_path.filename().string(), dep_name, dep_name + ".bin",
                                   candidates.size());
                        return false;
                    }
                }
            } catch (const std::exception &e) {
                fmt::print(stderr, "carbon: error: cannot parse '{}': {}\n", dci_path.string(),
                           e.what());
                return false;
            }
        }

        // Load the .bin itself.
        auto result = carbon::BinaryFile::from_path(bin_path);
        if (!result) {
            fmt::print(stderr, "carbon: error: cannot read '{}': {}\n", bin_path.string(),
                       result.error());
            return false;
        }
        if (!carbon::Globals::inst().load_module(std::move(*result))) {
            fmt::print(stderr, "carbon: error: failed to register '{}'\n", bin_path.string());
            return false;
        }
        fmt::print(fg(fmt::color::green), "carbon: loaded {}\n", bin_path.string());
        return true;
    }

} // namespace

int main(int argc, char *argv[]) {
    std::vector<std::string>           load_files;
    std::vector<std::filesystem::path> search_paths;
    std::vector<std::string>           positional;
    bool                               want_help = false;

    // ----- Parse CLI -----
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            want_help = true;
            break;
        }
        if (arg == "-I" || arg == "--include") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: error: '{}' requires a dir\n", arg);
                return 1;
            }
            search_paths.emplace_back(argv[++i]);
            continue;
        }
        if (arg == "-l" || arg == "--load") {
            if (i + 1 >= argc) {
                fmt::print(stderr, "carbon: error: '{}' requires a file\n", arg);
                return 1;
            }
            load_files.emplace_back(argv[++i]);
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            fmt::print(stderr, "carbon: error: unknown option '{}'\n", arg);
            print_usage(argv[0]);
            return 1;
        }
        positional.push_back(arg);
    }

    if (want_help || (load_files.empty() && positional.size() < 2)) {
        print_usage(argv[0]);
        return want_help ? 0 : 1;
    }

    // ----- Kernel -----
    bool            kernel_ok = false;
    VirtualMachine *vm_ptr = nullptr;

    try {
        auto &kernel = Kernel::instance();
        kernel_ok = kernel.initialize();
        if (kernel_ok) {
            vm_ptr = &kernel.virtual_machine();
        } else {
            lg::warn("carbon: Kernel::initialize() returned false — "
                     "falling back to a bare VirtualMachine");
        }
    } catch (const std::exception &e) {
        lg::warn("carbon: Kernel initialization failed ({}), "
                 "falling back to a bare VirtualMachine",
                 e.what());
    }

    VirtualMachine  local_vm;
    VirtualMachine &vm = vm_ptr ? *vm_ptr : local_vm;

    // ----- Load modules -----
    //
    // Order:
    //   1. Explicit -l files, in the order given.
    //   2. The first positional, if it is an existing file.
    //
    // No "prelude" guessing: carbon does not know about lib.bin. If a
    // module needs lib.bin, its .dci must say so, and the loader will
    // find lib.bin via .dci + search_paths.
    std::set<std::filesystem::path> already_loaded;

    auto load_one = [&](const std::string &path) -> bool {
        return load_module_recursive(path, already_loaded, search_paths);
    };

    for (const auto &path : load_files) {
        if (!load_one(path)) {
            if (kernel_ok) { Kernel::instance().shutdown(); }
            return 1;
        }
    }

    std::size_t pos_start = 0;
    if (!positional.empty() && std::filesystem::exists(positional[0])) {
        if (!load_one(positional[0])) {
            if (kernel_ok) { Kernel::instance().shutdown(); }
            return 1;
        }
        pos_start = 1;
    }

    if (positional.size() <= pos_start) {
        fmt::print(stderr, "carbon: error: no function name given\n");
        print_usage(argv[0]);
        if (kernel_ok) { Kernel::instance().shutdown(); }
        return 1;
    }

    const std::string    fn_name = positional[pos_start];
    std::vector<Variant> args;
    args.reserve(positional.size() - pos_start - 1);
    for (std::size_t i = pos_start + 1; i < positional.size(); ++i) {
        args.push_back(parse_argument(positional[i].c_str()));
    }

    // ----- Resolve function -----
    void *fn_ptr = Globals::inst().find_symbol_ptr(StringId(fn_name));
    if (fn_ptr == nullptr) {
        fmt::print(stderr, "carbon: error: function '{}' not found in Globals\n", fn_name);
        if (kernel_ok) { Kernel::instance().shutdown(); }
        return 1;
    }

    auto *lambda = reinterpret_cast<ScriptLambda *>(fn_ptr);

    // ----- Run -----
    fmt::print(fg(fmt::color::cyan), "carbon: calling {}({} args)\n", fn_name, args.size());

    Variant result;
    try {
        result = vm.execute_function(lambda, RunMode::Run, args);
        vm.purge();
    } catch (const std::exception &e) {
        fmt::print(stderr, "carbon: execution failed: {}\n", e.what());
        if (kernel_ok) { Kernel::instance().shutdown(); }
        return 1;
    }

    if (vm.is_error) {
        fmt::print(stderr, "carbon: VM error: {}\n", vm.break_reason);
        if (kernel_ok) { Kernel::instance().shutdown(); }
        return 1;
    }

    fmt::print(fg(fmt::color::green) | fmt::emphasis::bold, "{} = {}\n", fn_name,
               result.to_string());

    if (kernel_ok) { Kernel::instance().shutdown(); }
    return 0;
}