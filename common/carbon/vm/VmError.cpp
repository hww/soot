#include "carbon/vm/VmError.hpp"
#include "carbon/vm/StackFrame.hpp"

#include "fmt/format.h"

namespace carbon {

    // ========================================================================
    // ScriptException
    // ========================================================================

    std::string ScriptException::full_report() const {
        std::string out = fmt::format("ScriptException: {}", message);
        for (const auto &line : stacktrace_lines) { out += "\n  " + line; }
        return out;
    }

    // ========================================================================
    // VmError
    // ========================================================================

    VmError::VmError(Kind kind, std::string message, std::shared_ptr<StackFrame> frame,
                     const Instruction *failing_instr)
        : m_kind(kind), m_message(std::move(message)), m_origin(frame) {
        capture_stacktrace(frame, failing_instr);
    }

    void VmError::capture_stacktrace(const std::shared_ptr<StackFrame> &frame,
                                     const Instruction                 *failing_instr) {
        auto cursor = frame;
        bool first = true;

        while (cursor) {
            FrameInfo info;

            // Function name from the ScriptLambda header.
            if (cursor->byte_code != nullptr && cursor->byte_code->m_funcName != 0) {
                info.function_name = StringId(cursor->byte_code->m_funcName).to_string();
            } else {
                info.function_name = "<anonymous>";
            }

            info.frame_name = cursor->name.to_string();
            info.pc = cursor->pc;

            // Only the top frame carries the failing instruction and
            // the register dump. This keeps the report small and points
            // the reader at the exact site.
            if (first && failing_instr != nullptr) {
                info.instruction = fmt::format("{} {} {} {}", failing_instr->opcode_to_string(),
                                               static_cast<unsigned>(failing_instr->a),
                                               static_cast<unsigned>(failing_instr->b),
                                               static_cast<unsigned>(failing_instr->c));

                for (u32 i = 0; i < MAX_REGISTERS; ++i) {
                    const Variant &r = cursor->registers[i];
                    if (!r.is_null()) {
                        info.registers.emplace_back(static_cast<u8>(i), r.to_string());
                    }
                }
            }

            m_frames.push_back(std::move(info));
            first = false;
            cursor = cursor->get_parent();
        }

        // Build the ScriptException for script-level handling. Even
        // though no handler may catch it, the object is created and
        // available via exception_object() for the REPL to inspect.
        m_exception_object = std::make_shared<ScriptException>();
        m_exception_object->kind = m_kind;
        m_exception_object->message = m_message;
        m_exception_object->origin = m_origin;
        for (const auto &f : m_frames) {
            m_exception_object->stacktrace_lines.push_back(
                fmt::format("at {} ({}) pc={}", f.function_name, f.frame_name, f.pc));
            if (!f.instruction.empty()) {
                m_exception_object->stacktrace_lines.push_back(
                    fmt::format("    {}", f.instruction));
            }
        }
    }

    std::string VmError::full_report() const {
        std::string out;

        const char *kind_str = "Internal error";
        switch (m_kind) {
        case Kind::NullDeref: kind_str = "Null pointer dereference"; break;
        case Kind::TypeMismatch: kind_str = "Type mismatch"; break;
        case Kind::UndefinedFunction: kind_str = "Undefined function"; break;
        case Kind::DivisionByZero: kind_str = "Division by zero"; break;
        case Kind::NativeError: kind_str = "Native function error"; break;
        case Kind::ScriptError: kind_str = "Script error"; break;
        case Kind::Internal: kind_str = "Internal VM error"; break;
        }

        out += fmt::format("\n--- VM ERROR: {} ---\n", kind_str);
        out += fmt::format("{}\n", m_message);

        out += "\nStack trace (innermost first):\n";
        for (size_t i = 0; i < m_frames.size(); ++i) {
            const auto &f = m_frames[i];
            out += fmt::format("  [{}] {} ({})", i, f.function_name, f.frame_name);
            if (f.pc != 0 || !f.instruction.empty()) { out += fmt::format(" at pc={}", f.pc); }
            out += "\n";
            if (!f.instruction.empty()) { out += fmt::format("        {}\n", f.instruction); }
            if (!f.registers.empty()) {
                out += "        registers:\n";
                for (const auto &[idx, val] : f.registers) {
                    out += fmt::format("            r{} = {}\n", static_cast<unsigned>(idx), val);
                }
            }
        }

        return out;
    }

    VmError VmError::null_deref(const std::string &what, std::shared_ptr<StackFrame> frame,
                                const Instruction &instr) {
        return VmError(Kind::NullDeref, what, std::move(frame), &instr);
    }

    VmError VmError::type_mismatch(const std::string &what, std::shared_ptr<StackFrame> frame,
                                   const Instruction &instr) {
        return VmError(Kind::TypeMismatch, what, std::move(frame), &instr);
    }

    VmError VmError::undefined_function(const std::string &what, std::shared_ptr<StackFrame> frame,
                                        const Instruction &instr) {
        return VmError(Kind::UndefinedFunction, what, std::move(frame), &instr);
    }

    VmError VmError::division_by_zero(const std::string &what, std::shared_ptr<StackFrame> frame,
                                      const Instruction &instr) {
        return VmError(Kind::DivisionByZero, what, std::move(frame), &instr);
    }

    VmError VmError::native_error(const std::string &what, std::shared_ptr<StackFrame> frame,
                                  const Instruction *instr) {
        return VmError(Kind::NativeError, what, std::move(frame), instr);
    }

} // namespace carbon