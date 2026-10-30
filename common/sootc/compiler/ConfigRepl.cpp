// sootc/compiler/ConfigRepl.cpp
//
// Dynamic access to common::Config for the REPL. See ConfigRepl.hpp
// for why this lives in libsootc and not in libsootutils.
//
// This file knows:
//   * how to convert script::Object <-> C++ scalars,
//   * how to look up a field by name in FieldInfo and pick the
//     matching set_*/get_* on the struct,
//   * how to print one field.
//
// It does NOT know the names of individual fields: those live in
// Config.cpp, next to the fields themselves.

#include "sootc/compiler/ConfigRepl.hpp"

#include "script/Interpreter.hpp"

#include "util/Log.hpp"

#include "fmt/format.h"

#include <expected>
#include <sstream>
#include <string>
#include <vector>

namespace sootc::repl_commands {

    namespace {

        // ---------- script::Object -> C++ ----------

        std::expected<std::string, std::string> as_string(const script::Object &v,
                                                          std::string_view      field) {
            if (v.is_string()) return v.to_std_string();
            if (v.is_symbol()) return std::string(v.as_symbol().name_ptr);
            return std::unexpected(
                fmt::format("(opt): '{}' expects a string, got {}", field, v.print()));
        }

        std::expected<bool, std::string> as_bool(const script::Object &v, std::string_view field) {
            if (v.is_symbol()) {
                const std::string s = v.as_symbol().name_ptr;
                if (s == "#t" || s == "true") return true;
                if (s == "#f" || s == "false") return false;
            }
            if (v.is_integer()) return v.as_integer() != 0;
            return std::unexpected(
                fmt::format("(opt): '{}' expects #t or #f, got {}", field, v.print()));
        }

        std::expected<int64_t, std::string> as_int(const script::Object &v,
                                                   std::string_view      field) {
            if (v.is_integer()) return v.as_integer();
            return std::unexpected(
                fmt::format("(opt): '{}' expects an integer, got {}", field, v.print()));
        }

        std::expected<std::vector<std::filesystem::path>, std::string>
        as_path_list(const script::Object &v, std::string_view field) {
            std::vector<std::filesystem::path> out;
            if (v.is_string()) {
                out.emplace_back(v.to_std_string());
                return out;
            }
            if (v.is_pair() || v.is_null()) {
                auto cur = v;
                while (cur.is_pair()) {
                    const auto &item = cur.as_pair()->car;
                    if (!item.is_string()) {
                        return std::unexpected(fmt::format(
                            "(opt): '{}' expects a list of strings, got {}", field, item.print()));
                    }
                    out.emplace_back(item.to_std_string());
                    cur = cur.as_pair()->cdr;
                }
                if (!cur.is_null()) {
                    return std::unexpected(fmt::format("(opt): '{}' expects a proper list", field));
                }
                return out;
            }
            return std::unexpected(
                fmt::format("(opt): '{}' expects a string or list, got {}", field, v.print()));
        }

        // ---------- C++ -> script::Object ----------

        script::Object to_object(script::Interpreter &soot, const std::string &s) {
            return script::Object::make_string(s);
        }
        script::Object to_object(script::Interpreter &soot, bool b) {
            return soot.true_or_false(b);
        }
        script::Object to_object(script::Interpreter &, int64_t i) {
            return script::Object::make_integer(i);
        }
        script::Object to_object(script::Interpreter                      &soot,
                                 const std::vector<std::filesystem::path> &paths) {
            std::vector<script::Object> items;
            for (const auto &p : paths) items.push_back(to_object(soot, p.string()));
            return script::build_list(items);
        }

        script::Interpreter *g_interp = nullptr;

        // ---------- Field dispatch ----------
        //
        // set_field / get_field pick the matching set_*/get_* on the
        // struct by FieldInfo::type, and the matching struct by
        // FieldInfo::scope. Field names never appear here: they
        // live in Config.cpp.

        std::optional<std::string> set_field(common::Config &cfg, const common::FieldInfo &f,
                                             const script::Object &value) {
            auto dispatch = [&](auto &section) -> std::optional<std::string> {
                if (f.type == "string" || f.type == "path" || f.type == "enum") {
                    auto s = as_string(value, f.name);
                    if (!s) return s.error();
                    // "dump" is a special composite field: it sets
                    // four bool fields at once. Everything else goes
                    // through set_string.
                    if (f.name == "dump") return section.set_dump(*s);
                    return section.set_string(std::string(f.name), *s);
                }
                if (f.type == "bool" || f.type == "bool?") {
                    auto b = as_bool(value, f.name);
                    if (!b) return b.error();
                    return section.set_bool(std::string(f.name), *b);
                }
                if (f.type == "int") {
                    auto i = as_int(value, f.name);
                    if (!i) return i.error();
                    return section.set_int(std::string(f.name), *i);
                }
                if (f.type == "path-list") {
                    auto p = as_path_list(value, f.name);
                    if (!p) return p.error();
                    return section.set_path_list(std::string(f.name), *p);
                }
                return fmt::format("(opt): unknown type '{}' for field '{}'", f.type, f.name);
            };

            switch (f.scope) {
            case common::Scope::Common: return dispatch(cfg.common);
            case common::Scope::Compiler: return dispatch(cfg.compiler);
            case common::Scope::Host: return dispatch(cfg.host);
            }
            return "(opt): unknown scope";
        }

        std::optional<script::Object> get_field(const common::Config    &cfg,
                                                const common::FieldInfo &f) {
            if (!g_interp) return std::nullopt;

            auto dispatch = [&](auto &section) -> std::optional<script::Object> {
                if (f.type == "string" || f.type == "path" || f.type == "enum") {
                    if (f.name == "dump") {
                        auto s = section.get_dump();
                        if (!s) return std::nullopt;
                        return to_object(*g_interp, *s);
                    }
                    auto v = section.get_string(std::string(f.name));
                    if (!v) return std::nullopt;
                    return to_object(*g_interp, *v);
                }
                if (f.type == "bool" || f.type == "bool?") {
                    auto v = section.get_bool(std::string(f.name));
                    if (!v) return std::nullopt;
                    return to_object(*g_interp, *v);
                }
                if (f.type == "int") {
                    auto v = section.get_int(std::string(f.name));
                    if (!v) return std::nullopt;
                    return to_object(*g_interp, *v);
                }
                if (f.type == "path-list") {
                    auto v = section.get_path_list(std::string(f.name));
                    if (!v) return std::nullopt;
                    return to_object(*g_interp, *v);
                }
                return std::nullopt;
            };

            switch (f.scope) {
            case common::Scope::Common: return dispatch(cfg.common);
            case common::Scope::Compiler: return dispatch(cfg.compiler);
            case common::Scope::Host: return dispatch(cfg.host);
            }
            return std::nullopt;
        }

        void print_field(const common::Config &cfg, const common::FieldInfo &f) {
            auto              v = get_field(cfg, f);
            const std::string rendered = v ? v->print() : std::string("(unavailable)");

            fmt::print(stderr, "  {:<14} {:<10} {:<20} {}\n", std::string(f.name),
                       std::string(f.type), rendered, std::string(f.description));

            if (!f.allowed_values.empty()) {
                fmt::print(stderr, "  {:<14} {:<10} {:<20} allowed: {}\n", "", "", "",
                           std::string(f.allowed_values));
            }
        }

        const common::FieldInfo *find_field(std::string_view name) {
            static const std::vector<common::FieldInfo> kAll = common::Config::all_fields();
            for (const auto &f : kAll) {
                if (f.name == name) return &f;
            }
            return nullptr;
        }

    } // namespace

    void set_repl_interpreter(script::Interpreter *interp) { g_interp = interp; }

    bool opt_list(common::Config &cfg) {
        fmt::print(stderr, "\n");
        fmt::print(stderr, "  {:<14} {:<10} {}\n", "KEY", "TYPE", "VALUE");
        fmt::print(stderr, "  {:-<14} {:-<10} {:-<20}\n", "", "", "");

        auto section = [&](const char *title, const std::vector<common::FieldInfo> &fields) {
            fmt::print(stderr, "\n  {}\n", title);
            for (const auto &f : fields) print_field(cfg, f);
        };

        section("Common", common::CommonConfig::fields());
        section("Compiler", common::CompilerConfig::fields());
        section("Host", common::HostConfig::fields());

        fmt::print(stderr, "\n");
        return true;
    }

    bool opt_get(common::Config &cfg, const std::string &key) {
        const auto *f = find_field(key);
        if (!f) {
            fmt::print(stderr, "(opt): unknown key '{}'\n", key);
            return false;
        }
        auto v = get_field(cfg, *f);
        fmt::print(stderr, "{} = {}\n", f->name, v ? v->print() : "(unavailable)");
        return true;
    }

    bool opt_set(common::Config &cfg, const std::string &key, const script::Object &value) {
        const auto *f = find_field(key);
        if (!f) {
            fmt::print(stderr, "(opt): unknown key '{}'\n", key);
            return false;
        }
        if (!f->mutable_in_repl) {
            fmt::print(stderr, "(opt): '{}' is read-only; set it on the command line\n", key);
            return false;
        }
        auto err = set_field(cfg, *f, value);
        if (err) {
            fmt::print(stderr, "(opt): {}\n", *err);
            return false;
        }
        fmt::print(stderr, "{} = {}\n", key, value.print());
        return true;
    }

} // namespace sootc::repl