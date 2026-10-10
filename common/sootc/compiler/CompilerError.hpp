#pragma once

// ---------------------------------------------------------------------------
// CompilerError — structured error type for internal compiler bugs.
//
// This header is included from Compiler.hpp, which is included from many
// translation units. It must therefore be self-sufficient: every symbol
// used by the inline methods below has to be declared here, or pulled in
// by the includes at the top. In particular, fmt::color and fmt::emphasis
// live in fmt/color.h, NOT in fmt/format.h — include both.
// ---------------------------------------------------------------------------

#include "fmt/color.h"
#include "fmt/format.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace sootc {

    // -------------------------------------------------------------------------------------------
    //
    // CONVENTION
    //
    // 1. Internal errors (compiler bug, invalid state) -> CompilerError.
    //
    // 2. User errors (unknown symbol, wrong type) -> throw_compiler_error(form, ...)
    //    in Compiler, as we already have.
    //
    // 3. Invariants in hot code (assert, unreachable) — keep as std::logic_error or assert,
    //    they are not for the user and not for compiler debugging,
    //    but for catching programmer errors in C++.
    // -------------------------------------------------------------------------------------------
    //
    // Single format for internal compiler errors (bugs in the compiler itself,
    // not user errors).
    //
    // Usage:
    //   throw CompilerError("FileNode::generate")
    //       .where("repl file '<repl>'")
    //       .expected("at least one FunctionNode")
    //       .got("1 child: ConstNode");
    //
    class CompilerError : public std::runtime_error {
    public:
        explicit CompilerError(std::string where)
            : std::runtime_error(""), m_where(std::move(where)) {}

        CompilerError &where(std::string s) {
            m_details.emplace_back("where", std::move(s));
            return *this;
        }
        CompilerError &expected(std::string s) {
            m_details.emplace_back("expected", std::move(s));
            return *this;
        }
        CompilerError &got(std::string s) {
            m_details.emplace_back("got", std::move(s));
            return *this;
        }
        CompilerError &note(std::string s) {
            m_details.emplace_back("note", std::move(s));
            return *this;
        }

        CompilerError &at(std::string location) {
            m_details.emplace_back("at", std::move(location));
            return *this;
        }

        CompilerError &caused_by(std::string inner) {
            m_details.emplace_back("caused by", std::move(inner));
            return *this;
        }

        // Render the error to stdout in a compact, structured form.
        //
        // The color and emphasis specifiers come from fmt/color.h; the
        // include at the top of this file guarantees they are visible
        // here regardless of include order in the including translation
        // unit.
        void render() const {
            build_message();
            fmt::print(fg(fmt::color::indian_red) | fmt::emphasis::bold,
                       "\n─── COMPILER ERROR ─────────────────────────\n");
            if (!m_where.empty()) { // do not print an empty `where` line
                fmt::print(fg(fmt::color::indian_red), "{}\n", m_where);
            }
            for (const auto &[k, v] : m_details) {
                fmt::print(fg(fmt::color::dim_gray), "  {:<10}{}\n", k + ':', v);
            }
        }

        const char *what() const noexcept override {
            build_message();
            return m_message.c_str();
        }

    private:
        // Build the plain-text version of the message once and cache it.
        // Called from both render() and what(), so it must be idempotent.
        void build_message() const {
            if (!m_message.empty()) return;
            m_message = m_where;
            for (const auto &[k, v] : m_details) {
                m_message += "\n  ";
                m_message += k;
                m_message += ':';
                for (size_t i = k.size() + 1; i < 10; ++i) m_message += ' ';
                m_message += v;
            }
        }

        std::string                                      m_where;
        std::vector<std::pair<std::string, std::string>> m_details;
        mutable std::string                              m_message;
    };

} // namespace sootc