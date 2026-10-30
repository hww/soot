// sootc/compiler/ConfigRepl.hpp
//
// Dynamic access to common::Config for the REPL.
//
// This lives in libsootc, not in libsootutils: it needs
// script::Object, and libsootutils must stay free of the
// interpreter. The split is deliberate — Config describes the
// options, this file knows how to build and read script::Objects
// for them.

#pragma once

#include <optional>
#include <string>

#include "script/Interpreter.hpp"
#include "script/Object.hpp"

#include "util/Config.hpp"

namespace sootc::repl {

    // The REPL sets this once, before any (opt ...) runs.
    // Needed because (opt key) must return a script::Object, and
    // building one requires the interpreter's symbol table.
    void set_repl_interpreter(script::Interpreter* interp);

    // (opt)              -> list every field of every scope
    // (opt "key")        -> print one field's value and type
    // (opt "key" value)  -> change one field in memory
    //
    // The key is matched against FieldInfo::name across all scopes.
    // Changing a field marked mutable_in_repl == false is an error.
    //
    // These functions print to stderr and return true on success.
    bool opt_list(common::Config& cfg);
    bool opt_get (common::Config& cfg, const std::string& key);
    bool opt_set (common::Config& cfg, const std::string& key, const script::Object& value);

} // namespace sootc::repl