#pragma once

#include "common/sootc/Env/Env.hpp"
#include <string>

namespace sootc {

class Env;

/*!
 * An Env which manages the scope for (declare ...) statements.
 */
class DeclareEnv : public Env {
public:
    explicit DeclareEnv(EnvKind kind, Env* parent) : Env(kind, parent) {}
    virtual ~DeclareEnv() = default;
    virtual std::string print() const = 0;

    struct Settings {
        bool is_set = false;             // has the user set these with a (declare)?
        bool inline_by_default = false;  // if a function, inline when possible?
        bool save_code = true;           // if a function, should we save the code?
        bool allow_inline = false;       // should we allow the user to use this an inline function
        bool print_asm = false;          // should we print out the asm for this function?
    } settings;
};

} // namespace sootc