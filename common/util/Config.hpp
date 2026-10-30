// common/util/Config.hpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "CommonTypes.hpp" // SootPlatform
#include "util/Log.hpp"    // lg::level

namespace common {

    // ================================================================
    // Scope
    // ================================================================

    /// @brief Which tool owns an option.
    ///
    /// @details Every option belongs to exactly one scope. `sootc`
    ///          reads Common + Compiler; `carbon` reads Common + Host.
    ///          An option in the other tool's scope is silently
    ///          ignored: the same `soot.json` is shared by both
    ///          binaries, and neither should complain about keys it
    ///          does not understand.
    enum class Scope {
        Common,   ///< project_root, sidbase, platform, log, color, search_paths
        Compiler, ///< mode, dump, out_dir, repl, profile, runtime, do_files
        Host,     ///< listen, serve, hz, ticks, run_state_script, load_files
    };

    // ================================================================
    // FieldInfo
    // ================================================================

    /// @brief Description of one option, for (opt), help, and the
    ///        JSON reader.
    ///
    /// @details This is NOT an options table in the sense of "parse
    ///          everything from here". Types and values live in the
    ///          struct fields; the compiler checks them. FieldInfo
    ///          exists so that the code that iterates options —
    ///          `(opt)`, `print_help`, `load_json` — can do so
    ///          without knowing field names.
    struct FieldInfo {
        std::string_view name;            ///< CLI name, e.g. "log"
        std::string_view type;            ///< "bool", "int", "string", "path", "path-list", "enum"
        Scope            scope;           ///< Common, Compiler, or Host
        bool             mutable_in_repl; ///< false => (opt) prints "read-only"
        std::string_view description;     ///< one-line description for help
        std::string_view allowed_values;
    };

    // ================================================================
    // CommonConfig
    // ================================================================

    /// @brief Options shared by every tool in the tree.
    ///
    /// @details These are the things that describe "the project" and
    ///          "the log", not "the compiler" or "the host". Both
    ///          `sootc` and `carbon` read them.
    struct CommonConfig {
        std::filesystem::path project_root; ///< root for module resolution
        std::filesystem::path sidbase;      ///< string-ID table: load, then save
        SootPlatform          platform = SootPlatform::Default;
        lg::level             log_level = lg::level::warn;
        std::filesystem::path log_file;       ///< extra log destination; stderr is always on
        std::string           color = "auto"; ///< "auto" | "always" | "never"

        /// @brief Module search paths.
        /// @details Shared by `sootc` (finds `.sc`) and `carbon`
        ///          (finds `.bin`). Relative entries are resolved
        ///          against `project_root` by the callers, not here.
        std::vector<std::filesystem::path> search_paths = {".", "scripts", "src", "examples"};

        /// @brief The "table of contents" of this struct.
        static const std::vector<FieldInfo> &fields();

        /// @brief Typed setters.
        ///
        /// @details Each returns an error message on failure (unknown
        ///          field, wrong type, read-only field) and
        ///          `std::nullopt` on success. They live next to the
        ///          fields so that adding a field is a single-place
        ///          change. They do not know about `script::Object`:
        ///          the REPL layer converts before calling them.
        std::optional<std::string> set_string(const std::string &name, const std::string &value);
        std::optional<std::string> set_bool(const std::string &name, bool value);
        std::optional<std::string> set_int(const std::string &name, int64_t value);
        std::optional<std::string> set_path_list(const std::string                        &name,
                                                 const std::vector<std::filesystem::path> &value);

        /// @brief Composite "dump" field (no-op for CommonConfig).
        /// @details Declared on every struct because the REPL
        ///          dispatcher instantiates the same lambda for all
        ///          three. Returns "no 'dump' field" here.
        std::optional<std::string> set_dump(const std::string &value);

        /// @brief Typed getters.
        ///
        /// @details `std::nullopt` means "this struct has no such
        ///          field". The REPL prints "(unavailable)" for that.
        std::optional<std::string> get_string(const std::string &name) const;
        std::optional<bool>        get_bool(const std::string &name) const;
        std::optional<int64_t>     get_int(const std::string &name) const;
        std::optional<std::vector<std::filesystem::path>>
                                   get_path_list(const std::string &name) const;
        std::optional<std::string> get_dump() const;
    };

    // ================================================================
    // CompilerConfig
    // ================================================================

    /// @brief Options read by `sootc`.
    struct CompilerConfig {
        /// @brief Compilation mode: "compile" | "interpret" | "hybrid".
        /// @details Replaces the old --compile-only / --interpret-only
        ///          pair, which could conflict and could not express
        ///          HYBRID explicitly.
        std::string mode = "hybrid";

        // ----- Dumps -----
        //
        // Replaces the four flags --debug-ast/ir/asm and
        // --print-listing, which were the same thing under
        // different names. Set via the composite "dump" field
        // ("ast,ir,asm,listing") or individually through set_bool.
        bool dump_ast = false;
        bool dump_ir = false;
        bool dump_asm = false;
        bool print_listing = false;

        std::filesystem::path out_dir = "build"; ///< output directory for .bin/.h

        /// @brief Tri-state: "use soot.json" vs "force on/off".
        /// @details This is the one field where "not set" is
        ///          different from "set to the default value".
        std::optional<bool> repl;

        std::string user_profile = "#f"; ///< "#f" = no profile
        std::string runtime_url;         ///< "" = local, "null", "host:port"

        /// @brief Scripts to execute in the compiler session before
        ///        input files are compiled.
        /// @details Filled by `--do FILE` (repeatable) and by
        ///          `--run FILE` as an alias. Executed in order.
        std::vector<std::filesystem::path> do_files;

        static const std::vector<FieldInfo> &fields();

        std::optional<std::string> set_string(const std::string &name, const std::string &value);
        std::optional<std::string> set_bool(const std::string &name, bool value);
        std::optional<std::string> set_int(const std::string &name, int64_t value);
        std::optional<std::string> set_path_list(const std::string                        &name,
                                                 const std::vector<std::filesystem::path> &value);

        /// @brief Set the four dump flags from a comma-separated list.
        /// @param value "ast", "ast,ir", "listing", ...
        /// @return error on unknown kind, std::nullopt on success.
        std::optional<std::string> set_dump(const std::string &value);

        std::optional<std::string> get_string(const std::string &name) const;
        std::optional<bool>        get_bool(const std::string &name) const;
        std::optional<int64_t>     get_int(const std::string &name) const;
        std::optional<std::vector<std::filesystem::path>>
        get_path_list(const std::string &name) const;

        /// @brief Render the four dump flags back as "ast,ir,...".
        std::optional<std::string> get_dump() const;
    };

    // ================================================================
    // HostConfig
    // ================================================================

    /// @brief Options read by `carbon` (reserved for the host).
    struct HostConfig {
        /// @brief Address to listen on, "host:port" or just "port".
        /// @details Empty = "127.0.0.1:8181". `--port N` sets this to
        ///          "127.0.0.1:<N>".
        std::string listen;

        /// @brief Run the HTTP/REPL host loop. `--serve` / `--no-serve`.
        bool serve = false;

        int     hz = 60;    ///< tick rate for --run and --serve
        int64_t ticks = -1; ///< tick budget; -1 = unlimited

        /// @brief State-script name to spawn (`carbon --run NAME`).
        /// @details Not the same as `CompilerConfig::do_files`: this
        ///          is a state-script, not a do-file.
        std::string run_state_script;

        /// @brief `.bin` files to load explicitly (`carbon -l`).
        std::vector<std::filesystem::path> load_files;

        static const std::vector<FieldInfo> &fields();

        std::optional<std::string> set_string(const std::string &name, const std::string &value);
        std::optional<std::string> set_bool(const std::string &name, bool value);
        std::optional<std::string> set_int(const std::string &name, int64_t value);
        std::optional<std::string> set_path_list(const std::string                        &name,
                                                 const std::vector<std::filesystem::path> &value);

        /// @brief Composite "dump" field (no-op for HostConfig).
        std::optional<std::string> set_dump(const std::string &value);

        std::optional<std::string> get_string(const std::string &name) const;
        std::optional<bool>        get_bool(const std::string &name) const;
        std::optional<int64_t>     get_int(const std::string &name) const;
        std::optional<std::vector<std::filesystem::path>>
                                   get_path_list(const std::string &name) const;
        std::optional<std::string> get_dump() const;
    };

    // ================================================================
    // Config
    // ================================================================

    /// @brief The root config.
    ///
    /// @details Sub-configs are inline fields, not pointers: there is
    ///          exactly one copy of each value, and no way for two
    ///          parts of the program to disagree about "the" config.
    ///
    ///          A consumer that needs only its own section should
    ///          receive a const reference to that section, not a copy
    ///          and not a pointer. The root Config owns everything
    ///          and outlives every consumer.
    struct Config {
        CommonConfig   common;
        CompilerConfig compiler;
        HostConfig     host;

        /// @brief Common + Compiler + Host, in that order.
        /// @details Used by `(opt)` with no arguments and by
        ///          `print_help` when the tool wants to show
        ///          everything.
        static std::vector<FieldInfo> all_fields();
    };

    // ================================================================
    // Loading
    // ================================================================

    /// @brief Resolve the location of soot.json and read it into `cfg`.
    ///
    /// @param explicit_config If non-empty, use this file and do not
    ///                        search. This is what `--config` does.
    /// @param explicit_root   If non-empty, look for soot.json in
    ///                        this directory. This is what
    ///                        `--project-root` does.
    /// @param cfg             Filled with the values read from JSON.
    ///                        Missing keys keep their current values.
    /// @param resolved_json_path If non-null, receives the path that
    ///                        was actually read (or would have been).
    /// @return error message on failure, std::nullopt on success.
    ///
    /// @details A missing soot.json is not an error: defaults are
    ///          kept. A malformed soot.json is an error. Legacy
    ///          top-level keys are an error (they must be moved into
    ///          "common"/"compiler"/"host").
    std::optional<std::string> load_json(const std::string &explicit_config,
                                         const std::string &explicit_root, Config &cfg,
                                         std::filesystem::path *resolved_json_path = nullptr);

    /// @brief Apply environment variables.
    ///
    /// @details SOOT_* variables mirror the field names. Unknown
    ///          SOOT_* variables are ignored: they may belong to the
    ///          other tool.
    /// @return error message on failure, std::nullopt on success.
    std::optional<std::string> apply_env(Config &cfg);

    /// @brief Parse command-line arguments into `cfg`.
    ///
    /// @param argc, argv     The process arguments.
    /// @param cfg            Filled with the values from CLI.
    /// @param scope          Which tool is parsing: Compiler for
    ///                       `sootc`, Host for `carbon`. Common
    ///                       options are accepted by both.
    /// @param inputs         Positional arguments are appended here.
    /// @param no_config      Set to true if `--no-config` was given.
    /// @return error message on failure, std::nullopt on success.
    ///
    /// @details Unknown options are an error (the caller prints the
    ///          message and exits with code 2). An option from the
    ///          other tool is reported as such, not as "unknown".
    std::optional<std::string> apply_cli(int argc, char *argv[], Config &cfg, Scope scope,
                                         std::vector<std::string> &inputs, bool &no_config);

    /// @brief Print a help screen for the given scope.
    ///
    /// @details Common + one of Compiler/Host. Uses FieldInfo so the
    ///          help cannot drift from the struct. Bool options are
    ///          printed as `--[no-]name`.
    void print_help(Scope scope, const char *program_name);

} // namespace common