// common/util/Config.hpp
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "CommonTypes.hpp"   // SootPlatform
#include "script/Object.hpp" // script::Object, for (opt key value)
#include "util/Log.hpp"      // lg::level

namespace common {

    // ================================================================
    // Scopes
    // ================================================================
    //
    // Every option belongs to exactly one scope. sootc reads Common +
    // Compiler; carbon reads Common + Host. An option in the other
    // tool's scope is silently ignored (not an error): the same
    // soot.json is shared by both binaries, and neither should
    // complain about keys it does not understand.
    enum class Scope {
        Common,   // project_root, sidbase, platform, log, color
        Compiler, // mode, dump, out_dir, search_paths, repl, profile, runtime
        Host,     // listen, limits, resource paths (carbon only)
    };

    // ================================================================
    // FieldInfo — the "table of contents" of a config struct.
    // ================================================================
    //
    // This is NOT an options table in the sense of "parse everything
    // from here". Types and values live in the struct fields, and the
    // compiler checks them. FieldInfo exists only so that:
    //
    //   * (opt) with no arguments knows what to list;
    //   * print_help knows what to print;
    //   * the JSON reader knows which keys are "ours" (warn) and
    //     which are "the other tool's" (ignore silently);
    //   * (opt key value) knows whether a field may be changed
    //     in the REPL at all.
    //
    // Keeping it next to the struct means a rename of a field shows
    // up in the same file, and the compiler will not let it drift.
    struct FieldInfo {
        std::string_view name;
        std::string_view type; // "bool", "int", "string", "path", "path-list", "enum:mode", ...
        Scope            scope;
        bool             mutable_in_repl; // false => (opt) prints "read-only"
        std::string_view description;
    };

    // ================================================================
    // CommonConfig
    // ================================================================
    struct CommonConfig {
        std::filesystem::path project_root;
        std::filesystem::path sidbase;
        SootPlatform          platform = SootPlatform::Default;
        lg::level             log_level = lg::level::warn;
        std::filesystem::path log_file;
        std::string           color = "auto"; // "auto" | "always" | "never"
        
        // Module search paths. Shared by sootc (finds .sc) and
        // carbon (finds .bin). Relative entries are resolved against
        // project_root by the callers, not here.
        std::vector<std::filesystem::path>   search_paths = {".", "scripts", "src", "examples"};

        static const std::vector<FieldInfo> &fields();

        // Dynamic access for (opt key value) / (opt key).
        // Returns an error message on failure (unknown key, wrong
        // type, read-only field). Returns std::nullopt on success.
        std::optional<std::string> set(const std::string &name, const script::Object &value);

        // Returns std::nullopt if the key is unknown.
        std::optional<script::Object> get(const std::string &name) const;
    };

    // ================================================================
    // CompilerConfig
    // ================================================================
    struct CompilerConfig {
        // Compilation mode. Replaces the old --compile-only /
        // --interpret-only pair, which could conflict and could not
        // express HYBRID explicitly.
        std::string mode = "hybrid"; // "compile" | "interpret" | "hybrid"

        // Dumps. Replaces the four flags --debug-ast/ir/asm and
        // --print-listing, which were the same thing under different
        // names.
        bool dump_ast = false;
        bool dump_ir = false;
        bool dump_asm = false;
        bool print_listing = false;

        std::filesystem::path              out_dir = "build";

        // Tri-state: nullopt = "use soot.json", true/false = "force".
        // This is the one field where "not set" is different from
        // "set to the default value".
        std::optional<bool> repl;

        std::string user_profile = "#f";

        // "" = local VM, "null" = NullRuntime, "host:port" = remote.
        std::string runtime_url;

        // Scripts to execute in the compiler session before input
        // files are compiled. Repeated --do (or --run) appends here;
        // scripts run in the order given.
        std::vector<std::filesystem::path>   run_scripts;

        static const std::vector<FieldInfo> &fields();

        std::optional<std::string>    set(const std::string &name, const script::Object &value);
        std::optional<script::Object> get(const std::string &name) const;
    };

    // ================================================================
    // HostConfig
    // ================================================================
    //
    // Reserved for carbon. sootc does not read these fields, and
    // silently ignores the "host" section of soot.json.
    struct HostConfig {
        // Address to listen on, "host:port" or just "port". Empty =
        // 127.0.0.1:8181. --port N sets this to "127.0.0.1:<N>".
        std::string listen;

        // Run the HTTP/REPL host loop. --serve / --no-serve.
        bool serve = false;

        // Tick rate and budget for --run and --serve.
        int     hz = 60;
        int64_t ticks = -1;

        // State-script name to spawn (carbon --run NAME).
        std::string run_state_script;

        // .bin files to load explicitly (carbon -l/--load).
        std::vector<std::filesystem::path> load_files;

        static const std::vector<FieldInfo> &fields();
        std::optional<std::string>    set(const std::string &name, const script::Object &value);
        std::optional<script::Object> get(const std::string &name) const;
    };

    // ================================================================
    // Config — the root.
    // ================================================================
    //
    // One owner. Sub-configs are inline fields, not pointers: there
    // is exactly one copy of each value, and no way for two parts of
    // the program to disagree about "the" config.
    //
    // A consumer that needs only its own section should receive a
    // const reference to that section, not a copy and not a pointer.
    // The root Config owns everything and outlives every consumer.
    struct Config {
        CommonConfig   common;
        CompilerConfig compiler;
        HostConfig     host;

        // Common + Compiler + Host, in that order. Used by (opt)
        // with no arguments and by print_help when the tool wants
        // to show everything.
        static std::vector<FieldInfo> all_fields();
    };

    // ================================================================
    // Loading
    // ================================================================
    //
    // Layers, lowest to highest priority:
    //
    //   1. Struct defaults (the field initializers above).
    //   2. soot.json (or the file given with --config).
    //   3. Environment variables (SOOT_LOG, SOOT_RUNTIME, ...).
    //   4. Command-line arguments.
    //
    // Each layer only writes the keys it actually has. A key that
    // is absent from a layer is left as the previous layer set it.
    //
    // All four functions return an error message on failure and
    // std::nullopt on success. On failure, `cfg` may have been
    // partially updated; the caller should treat it as invalid.

    // Resolve the location of soot.json and read it into `cfg`.
    //
    //   * explicit_config: if non-empty, use this file and do not
    //     search. This is what --config does.
    //   * explicit_root:   if non-empty, look for soot.json in this
    //     directory. This is what --project-root does.
    //   * otherwise:       search upwards from CWD for soot.json.
    //
    // A missing soot.json is not an error: defaults are kept.
    // A malformed soot.json is an error.
    std::optional<std::string> load_json(const std::string &explicit_config,
                                         const std::string &explicit_root, Config &cfg,
                                         std::filesystem::path *resolved_json_path = nullptr);

    // Apply environment variables. Unknown SOOT_* variables are
    // ignored: they may belong to the other tool.
    std::optional<std::string> apply_env(Config &cfg);

    // Parse command-line arguments into `cfg`. Recognises only the
    // keys this binary owns; unknown flags are an error (the caller
    // prints them and exits with code 2).
    //
    // Positional arguments (input files) are appended to `inputs`.
    std::optional<std::string> apply_cli(int argc, char *argv[], Config &cfg, Scope scope,
                                         std::vector<std::string> &inputs, bool &no_config);

    // ================================================================
    // (opt) — dynamic access for the REPL.
    // ================================================================
    //
    // (opt)              -> list every field of every scope
    // (opt "key")        -> print one field's value and type
    // (opt "key" value)  -> change one field in memory
    //
    // The key is matched against FieldInfo::name across all scopes.
    // Changing a field marked mutable_in_repl == false is an error.
    //
    // These functions print to stderr and return a status code, so
    // that the REPL can show the result without extra plumbing.
    bool opt_list(Config &cfg);
    bool opt_get(Config &cfg, const std::string &key);
    bool opt_set(Config &cfg, const std::string &key, const script::Object &value);

    // ================================================================
    // help
    // ================================================================
    //
    // Print a help screen for the given scope (Common + one of
    // Compiler/Host). Uses FieldInfo so the help cannot drift from
    // the struct.
    void print_help(Scope scope, const char *program_name);

} // namespace common