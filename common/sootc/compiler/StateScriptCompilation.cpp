// sootc/compiler/StateScriptCompilation.cpp
//
// Compilation of state-script handler bodies and track steps into
// named FunctionNodes owned by the FileNode. Called from
// compile_internal after all top-level forms have been processed
// and before FileNode::generate() runs.

#include "sootc/compiler/Compiler.hpp"

#include "sootc/compiler/CompilerError.hpp"
#include "sootc/compiler/NodeBuilder.hpp"

#include "sootc/node/FileNode.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/Node.hpp"
#include "sootc/node/StateScriptNode.hpp"

#include "script/Interpreter.hpp"
#include "script/Object.hpp"

#include "type_system/TypeSystem.hpp"

#include "util/Log.hpp"

#include "fmt/format.h"

#include <string>
#include <vector>

namespace sootc {

    namespace {

        // Build a (lambda () body) form from an arbitrary body form.
        // If the form is already a lambda, use it as-is.
        script::Object ensure_lambda(script::Interpreter &soot, const script::Object &body) {
            if (body.is_pair() && body.as_pair()->car.is_symbol()) {
                const std::string head = body.as_pair()->car.as_symbol();
                if (head == "lambda" || head == "function") { return body; }
            }
            // Wrap: (lambda () body)
            return script::Object::make_pair(
                soot.intern("lambda"),
                script::Object::make_pair(
                    script::Object::make_null(),
                    script::Object::make_pair(body, script::Object::make_null())));
        }

        // (lambda () form...) from a list of forms: implicit begin, ONE lambda.
        script::Object lambda_from_forms(script::Interpreter               &soot,
                                         const std::vector<script::Object> &forms) {
            return script::Object::make_pair(
                soot.intern("lambda"),
                script::Object::make_pair(script::Object::make_null(), script::build_list(forms)));
        }

        std::string handler_kind_name(StateHandler kind) {
            switch (kind) {
            case StateHandler::ENTER: return "enter";
            case StateHandler::EXIT: return "exit";
            case StateHandler::CODE: return "code";
            case StateHandler::UPDATE: return "trans";
            case StateHandler::POST: return "post";
            case StateHandler::EVENT: return "event";
            }
            return "unknown";
        }

    } // namespace

    void Compiler::compile_state_script_bodies(StateScriptNode *ss, FileNode *file,
                                               NodeBuilder &builder) {
        const std::string &type_name = ss->type().base_type();

        Type *t = m_ts.lookup_type_no_throw(type_name);
        if (!t) {
            lg::warn("compile_state_script_bodies: unknown type '{}'", type_name);
            return;
        }

        // Compile one lambda form into a named FunctionNode owned by the file.
        auto emit = [&](const std::string &name, const script::Object &lambda_form,
                        const script::Object &where, const std::string &what) {
            try {
                auto  node = builder.build_lambda(lambda_form, file);
                auto *fn = dynamic_cast<FunctionNode *>(node.get());
                if (!fn) {
                    lg::warn("compile_state_script_bodies: '{}' did not produce a FunctionNode",
                             name);
                    return;
                }
                fn->set_name(name);
                FunctionNode *raw = fn;
                node.release();
                file->add_child(std::unique_ptr<Node>(raw));
                file->bind(name, raw);
            } catch (const std::exception &e) {
                throw make_error(where, fmt::format("compiling {}", what))
                    .expected("a compilable lambda body")
                    .got(e.what());
            }
        };

        for (const auto &[state_name, def] : t->state_defs()) {
            // ---- Handlers ----
            for (const auto &h : def.handlers) {
                if (h.body_ast.is_null()) continue;

                const std::string base =
                    fmt::format("{}-{}-{}", type_name, state_name, handler_kind_name(h.kind));
                const std::string name = (h.kind == StateHandler::EVENT)
                                             ? fmt::format("{}-event-{}", base, h.event_id)
                                             : base;

                emit(name, ensure_lambda(m_soot, h.body_ast), h.body_ast,
                     fmt::format("handler '{}'", name));
            }

            // ---- Tracks (belong to the state) ----
            for (const auto &tr : def.tracks) {
                const std::string prefix =
                    fmt::format("{}-{}-track-{}", type_name, state_name, tr.name);

                // On-begin coroutine: ONE lambda for the whole body.
                if (!tr.body.empty()) {
                    emit(prefix + "-main", lambda_from_forms(m_soot, tr.body), tr.body.front(),
                         fmt::format("track '{}' body", tr.name));
                }

                // Reactive frame triggers: one lambda each, named by index.
                for (size_t i = 0; i < tr.frames.size(); ++i) {
                    const auto &ft = tr.frames[i];
                    emit(fmt::format("{}-frame-{}", prefix, i), lambda_from_forms(m_soot, ft.body),
                         ft.body.front(), fmt::format("track '{}' frame {}", tr.name, ft.frame));
                }
            }
        }

        lg::debug("compile_state_script_bodies: '{}' compiled", type_name);
    }

} // namespace sootc