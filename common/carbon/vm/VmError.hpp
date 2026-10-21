// carbon/vm/VmError.hpp
#pragma once

#include "CommonTypes.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/vm/Instructions.hpp"

#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace carbon {

    struct StackFrame; // forward

    // ========================================================================
    // ScriptException
    // ========================================================================
    //
    // The object that a script-level handler sees when it catches a VM
    // error. It is a plain C++ object, owned by VmError via shared_ptr,
    // NOT a GC object. See make_script_exception() for the lifetime
    // rules.
    //
    struct ScriptException {
        enum class Kind {
            NullDeref,
            TypeMismatch,
            UndefinedFunction,
            DivisionByZero,
            NativeError,
            ScriptError, // (raise ...) from a script
            Internal,
        };

        Kind                        kind = Kind::Internal;
        std::string                 message;
        std::vector<std::string>    stacktrace_lines; // human-readable
        std::shared_ptr<StackFrame> origin;           // where it happened
        std::shared_ptr<StackFrame> handler;          // catch frame (may be null)

        [[nodiscard]] std::string full_report() const;
    };

    // ========================================================================
    // VmError
    // ========================================================================
    //
    // The C++-side exception/error object. Used for both:
    //   * `throw VmError(...)` from C++ code (rare; used by native
    //     functions that want to signal a VM-level failure),
    //   * `set_error(...)` in VirtualMachine (the normal path: the VM
    //     records the error, does NOT throw, and unwinds later).
    //
    // A VmError carries a complete snapshot of the VM stack at the
    // moment of the failure: function names, PCs, the failing
    // instruction, and the registers of the origin frame.
    //
    class VmError : public std::exception {
    public:
        using Kind = ScriptException::Kind;

        /// One frame in the VM stacktrace.
        struct FrameInfo {
            std::string function_name; // from ScriptLambda::m_funcName, or "<anonymous>"
            std::string frame_name;    // StackFrame::name (StringId), or "<none>"
            u32         pc = 0;
            std::string instruction; // "LoadFloat r1, r2" — only on origin
            // Registers of the origin frame, captured at throw time.
            // Empty for non-origin frames.
            std::vector<std::pair<u8, std::string>> registers;
        };

        VmError() = default;

        VmError(Kind kind, std::string message, std::shared_ptr<StackFrame> frame = nullptr,
                const Instruction *failing_instr = nullptr);

        [[nodiscard]] Kind                          kind() const noexcept { return m_kind; }
        [[nodiscard]] const std::vector<FrameInfo> &frames() const noexcept { return m_frames; }
        [[nodiscard]] const std::shared_ptr<StackFrame> &origin() const noexcept {
            return m_origin;
        }
        [[nodiscard]] const std::string &message() const noexcept { return m_message; }
        [[nodiscard]] const std::shared_ptr<ScriptException> &exception_object() const noexcept {
            return m_exception_object;
        }

        [[nodiscard]] std::string full_report() const;

        const char *what() const noexcept override { return m_message.c_str(); }

        // Convenience factories.
        static VmError null_deref(const std::string &what, std::shared_ptr<StackFrame> frame,
                                  const Instruction &instr);
        static VmError type_mismatch(const std::string &what, std::shared_ptr<StackFrame> frame,
                                     const Instruction &instr);
        static VmError undefined_function(const std::string          &what,
                                          std::shared_ptr<StackFrame> frame,
                                          const Instruction          &instr);
        static VmError division_by_zero(const std::string &what, std::shared_ptr<StackFrame> frame,
                                        const Instruction &instr);
        static VmError native_error(const std::string &what, std::shared_ptr<StackFrame> frame,
                                    const Instruction *instr);

    private:
        void capture_stacktrace(const std::shared_ptr<StackFrame> &frame,
                                const Instruction                 *failing_instr);

        Kind                             m_kind = Kind::Internal;
        std::string                      m_message;
        std::shared_ptr<StackFrame>      m_origin;
        std::vector<FrameInfo>           m_frames;
        std::shared_ptr<ScriptException> m_exception_object;
    };

    // Backwards-compatible aliases for existing code.
    using VmTypeError = VmError;
    using VmResolvingError = VmError;

} // namespace carbon