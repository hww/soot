
#include "carbon/vm/NativeFunc.hpp"
#include "carbon/vm/NativeProcess.hpp"
#include "CommonTypes.hpp"
#include "util/Log.hpp"
#include "lib/StringId.hpp"
#include "fmt/format.h"       // fmt::format, fmt::vformat, fmt::dynamic_format_arg_store
#include "fmt/args.h"         // fmt::dynamic_format_arg_store (в некоторых версиях fmt)
#include "fmt/core.h"         // базовые функции fmt (fmt::print)     
#include <iostream>
#include <Carbon.cpp>

namespace carbon {

    // ============================================================================
    // Native Function Registry Implementation
    // ============================================================================

    NativeFunctionRegistry& NativeFunctionRegistry::get_instance() {
        static NativeFunctionRegistry instance;
        return instance;
    }
    
    
    void NativeFunctionRegistry::register_function(StringId name, NativeFunction func) {
        functions_[name] = func;
        lg::debug("Registered native function: {}", name);
    }

    void NativeFunctionRegistry::register_function(const std::string& name, NativeFunction func) {
        register_function(StringId(name), func);
    }

    void NativeFunctionRegistry::register_function(const char* name, NativeFunction func) {
        register_function(StringId(name), func);
    }

    NativeFunction NativeFunctionRegistry::find_function(const StringId name) const {
        auto it = functions_.find(name);
        return it != functions_.end() ? it->second : nullptr;
    }

    NativeFunction NativeFunctionRegistry::find_function_by_name(const std::string& name) const {
        return find_function(StringId(name));
    }

    // ============================================================================
    // Built-in Native Functions
    // ============================================================================
    // ---- Общая функция для форматирования аргументов ----
    static std::string format_impl(u32 argc, const Variant *argv, const char *func_name) {
        if (argc == 0) { return std::string(); }

        // ---- argv[0] — format string ----
        if (!argv[0].is_ptr()) {
            // Если первый аргумент не строка — печатаем всё как есть
            // (чтобы не падать на (print 42)).
            std::string result;
            for (u32 i = 0; i < argc; ++i) {
                if (i > 0) result += " ";
                result += fmt::format("{}", argv[i]);
            }
            return result;
        }

        const char *fmt_str = static_cast<const char *>(argv[0].get_ptr());

        // ---- Остальные argv[1..N] — varargs для fmt ----
        fmt::dynamic_format_arg_store<fmt::format_context> store;
        for (u32 i = 1; i < argc; ++i) {
            // Push the Variant itself, not its unpacked value. This way
            // fmt dispatches to formatter<Variant>, which understands
            // {:p}, {:sid}, {:t=...}, {:x}, {:b}, {:f}, and {}.
            store.push_back(argv[i]);
        }

        // ---- Один вызов fmt::vformat — формирует строку целиком ----
        try {
            return fmt::vformat(fmt_str, store);
        } catch (const fmt::format_error &e) {
            // Если формат сломан — бросаем исключение с сообщением.
            throw std::runtime_error(fmt::format("[{}] format string `{}` contains error: {}\n",
                                                     func_name, fmt_str, e.what()));
        }
    }

    // ---- native_print: использует format_impl и печатает результат ----
    Variant native_print(u32 argc, const Variant *argv) {
        fmt::print("{}", format_impl(argc, argv, "print"));
        return Variant(true);
    }

    // ---- native_println: печатает результат + перевод строки ----
    Variant native_println(u32 argc, const Variant *argv) {
        fmt::print("{}\n", format_impl(argc, argv, "println"));
        return Variant(true);
    }

    // ---- native_format: возвращает отформатированную строку ----
    Variant native_format(u32 argc, const Variant *argv) {
        return Variant(format_impl(argc, argv, "format"));
    }

    // ============================================================================
    // String ID
    // ============================================================================
    
    Variant native_sid_to_string(u32 argc, const Variant* argv) {
        if (argc == 0) { 
              throw std::runtime_error(fmt::format("sid_to_string expects arg0 sid, got nothing\n"));
        }
        auto arg0 = argv[0];
        if (!arg0.is_int()) {
            throw std::runtime_error(
                fmt::format("sid_to_string expects arg0 sid, got {}\n", arg0.to_string()));
        }
        auto str = StringIdManager::instance().get_string(arg0.to_int());
        return Variant(str);
    }

    Variant native_string_to_sid(u32 argc, const Variant* argv) {
        if (argc == 0) {
            throw std::runtime_error(fmt::format("string_to_sid expects arg0 sid, got nothing\n"));
        }
        auto arg0 = argv[0];
        if (!arg0.is_ptr()) {
            throw std::runtime_error(
                fmt::format("string_to_sid expects arg0 string, got {}\n", arg0.to_string()));
        }
        return Variant(StringId(arg0.to_string()));
    }

    // ============================================================================
    // Module loading
    // ============================================================================

    // (load-module "name")
    //
    // Search order for "name.bin":
    //   1. Absolute path — used as-is.
    //   2. CWD / name.bin
    //   3. PROJECT / name.bin
    //   4. For each dir in CarbonConfig::search_paths:
    //        PROJECT / dir / name.bin
    //
    // Loading is idempotent: if the module is already in ModuleRegistry
    // (under the same logical name), this is a no-op.
    //
    // The function returns #t on success, #f on failure. Errors are
    // logged via lg::error but do not throw: a script that calls
    // (load-module "missing") should get #f and continue, not crash
    // the VM.
    Variant native_load_module(u32 argc, const Variant *argv) {
        if (argc == 0 || !argv[0].is_ptr()) {
            lg::error("load-module: expected a string argument, got {}",
                      argc == 0 ? "<nothing>" : argv[0].to_string());
            return Variant(false);
        }

        // A string Variant holds a Pointer to a null-terminated const char*.
        // Variant::to_string() on a Pointer prints "ptr:0x...", so we must
        // dereference it explicitly. This matches how native_print and
        // native_format read their format string.
        const char *raw = static_cast<const char *>(argv[0].get_ptr());
        if (!raw) {
            lg::error("load-module: null string pointer");
            return Variant(false);
        }
        const std::string name(raw);

        if (name.empty()) {
            lg::error("load-module: empty module name");
            return Variant(false);
        }

        Carbon *carbon = Carbon::current();
        if (!carbon) {
            lg::error("load-module: no active Carbon context");
            return Variant(false);
        }

        namespace fs = std::filesystem;

        // Build the list of candidate paths for "<name>.bin".
        std::vector<fs::path> candidates;

        fs::path given(name);
        if (given.is_absolute()) {
            // Absolute: only one candidate.
            candidates.push_back(given);
            if (given.extension() != ".bin") { candidates.back().replace_extension(".bin"); }
        } else {
            // Relative: try as-is, then PROJECT, then search_paths.
            fs::path as_given = given;
            if (as_given.extension() != ".bin") { as_given.replace_extension(".bin"); }
            candidates.push_back(as_given);

            fs::path project = file_util::get_path(file_util::PathType::PROJECT);
            candidates.push_back(project / as_given);

            for (const auto &dir : carbon->config().search_paths) {
                fs::path dir_abs = dir.is_absolute() ? dir : (project / dir);
                candidates.push_back(dir_abs / as_given);
            }
        }

        // Find the first candidate that exists.
        fs::path found;
        for (const auto &c : candidates) {
            std::error_code ec;
            if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) {
                found = c;
                break;
            }
        }

        if (found.empty()) {
            std::string tried;
            for (const auto &c : candidates) { tried += "\n  " + c.string(); }
            lg::error("load-module: cannot find '{}'. Tried:{}", name, tried);
            return Variant(false);
        }

        // Idempotence: if the module is already loaded (by canonical
        // path), skip. Carbon::load_module_with_imports already does
        // this internally, but we check here so the log message is
        // accurate.
        std::error_code ec;
        fs::path        canonical = fs::weakly_canonical(found, ec);
        if (ec) canonical = found;

        if (carbon->modules()->contains_path(canonical)) {
            lg::debug("load-module: '{}' already loaded", canonical.string());
            return Variant(true);
        }

        // Load with imports. load_module_with_imports reads the .dci
        // and recursively loads declared imports before running
        // top-level. If the .dci is absent, only the .bin is loaded.
        //
        // search_paths passed to load_module_with_imports are the
        // SAME list used to resolve the top-level module, so imports
        // resolve consistently.
        std::vector<fs::path> search_paths;
        for (const auto &dir : carbon->config().search_paths) {
            search_paths.push_back(dir.is_absolute()
                                       ? dir
                                       : (file_util::get_path(file_util::PathType::PROJECT) / dir));
        }

        if (!carbon->load_module_with_imports(found, search_paths)) {
            lg::error("load-module: failed to load '{}'", found.string());
            return Variant(false);
        }

        lg::info("load-module: loaded '{}'", found.string());
        return Variant(true);
    }

    // ============================================================================
    // Registry Initialization
    // ============================================================================

    void NativeFunctionRegistry::initialize_builtins() {
        // Basic I/O
        register_function("print", native_print);
        register_function("println", native_println);
        register_function("format", native_println);

        // String ID
        register_function("string-to-sid", native_string_to_sid);
        register_function("sid-to-string", native_sid_to_string);

        // Load modules
        register_function("load-module", native_load_module);

        // Process control
        initialize_process_natives();

        lg::info("Initialized {} built-in native functions", functions_.size());
    }

} // namespace vm