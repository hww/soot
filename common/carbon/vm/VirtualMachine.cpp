#include "carbon/vm/VirtualMachine.hpp"
#include "carbon/file/Export.hpp"
#include "carbon/vm/StackFrame.hpp"
#include "carbon/file/DCScript.hpp"
#include "carbon/file/Globals.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/Variant.hpp"
#include "carbon/Carbon.hpp"
#include "carbon/kernel/Process.hpp"
#include "util/Log.hpp"

using namespace carbon;
using namespace carbon;


namespace carbon {

    // ------------------------------------------------------------------------
    // Process Management (НОВОЕ - согласно нашему базису)
    // ------------------------------------------------------------------------

    // Исполнение кода
    Variant VirtualMachine::execute_function(ScriptLambda *script_lambda, RunMode mode) {
        if (!script_lambda) {
            lg::error("Cannot execute null FunctionDesc");
            return Variant();
        }

        auto current_frame = std::make_shared<StackFrame>(script_lambda, nullptr);

        return execute(current_frame, mode);
    }

    Variant VirtualMachine::execute_function(ScriptLambda *script_lambda, RunMode mode,
                                             const std::vector<Variant> &args) {
        if (!script_lambda) {
            lg::error("Cannot execute null FunctionDesc");
            return Variant();
        }
        auto current_frame = std::make_shared < StackFrame>(script_lambda, nullptr);

        // ---- Install arguments into r24, r25, ... ----
        //
        // Mirrors the calling convention used by Opcode::Call: arguments live
        // in registers ARG_REGISTERS_OFFSET + 0, +1, ... and argc is set on the
        // frame. Prologue then copies them into local registers.
        current_frame->argc = static_cast<u32>(args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            current_frame->get_register(ARG_REGISTERS_OFFSET + i) = args[i];
        }

        return execute(current_frame, mode);
    }

    Variant VirtualMachine::execute(std::shared_ptr<StackFrame> stack_frame, RunMode mode) {
        current_frame = stack_frame;
        return execute(mode);
    }

    // Исполнение кода связанного со стек фреймом
    Variant VirtualMachine::execute(RunMode mode) {
        m_has_error = false;
        m_pending_error = VmError{};
        is_break = false;
        Variant final_result;

        // Исполнение кода
        while (current_frame != nullptr) {
            if (is_break) return Variant();
            
            if (m_has_error) {
                // An earlier instruction set the error flag. Stop
                // executing. Do NOT reset current_frame: the stacktrace
                // in m_pending_error references it, and the host may want
                // to inspect the frame.
                return Variant();
            }

            Instruction instr = current_frame->get_next_instruction();

            if (true || enable_debug_log) {
                std::string logstring;
                for (u32 i = 0; i < current_frame->get_registers_count(); i++) {
                    logstring +=
                        fmt::format("R{}={} ", i, current_frame->get_register(i).to_string());
                }
                lg::debug("PC={} : {} ;; {}", current_frame->pc - 1, instr.to_string(),
                            logstring);
            }

            try {

                // Исколнение конкретной инструкции
                switch (instr.opcode) {
                // ============================================================
                // Control Flow Instructions (0x0*)
                // ============================================================
                case Opcode::Return: {
                    Variant return_value = current_frame->get_register(instr.a);

                    auto parent_frame = current_frame->get_parent();

                    // If the parent is a container frame (TrackFrame, StateFrame)
                    // — i.e. it has no bytecode — do not descend into it. Containers
                    // are not functions; get_next_instruction would assert on them.
                    // Just return the value and stop.
                    if (!parent_frame || parent_frame->code_ptr == nullptr) {
                        final_result = return_value;
                        current_frame.reset();
                    } else {
                        parent_frame->get_register(current_frame->ret_num) = return_value;
                        current_frame = parent_frame;
                    }

                    if (mode == RunMode::StepOut) is_break = true;
                    break;
                }

                case Opcode::Move: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = src;
                    break;
                }
                case Opcode::Call: {
                    Variant &func_var = current_frame->get_register(instr.b);

                    void *func_ptr = nullptr;

                    if (func_var.is_ptr()) {
                        func_ptr = func_var.get_ptr();
                    } else if (func_var.is_int()) {
                        StringId sid = func_var.get_sid();
                        func_ptr = carbon->globals()->find_symbol_ptr(sid);

                        if (!func_ptr) {
                            set_error(
                                VmError::Kind::UndefinedFunction,
                                fmt::format(
                                    "Call: function '{}' (SID 0x{:016X}) not found in Globals",
                                    sid.to_cstring(), static_cast<u64>(sid)),
                                &instr);
                            break;
                        }

                        // Кэшируем разрешённый указатель в регистр, чтобы повторные
                        // вызовы не дёргали Globals.
                        func_var = Variant(func_ptr, RuntimeType::Pointer);
                    } else {
                        set_error(VmError::Kind::TypeMismatch,
                                  fmt::format("Call: unexpected function value in r{} "
                                              "(not pointer, not int): got {}",
                                              static_cast<unsigned>(instr.b),
                                              value_type_to_string(func_var.get_type())),
                                  &instr);
                        break;
                    }

                    ScriptLambda *target_code = reinterpret_cast<ScriptLambda *>(func_ptr);

                    auto new_frame = std::make_shared<StackFrame>(target_code, current_frame);

                    new_frame->ret_num = instr.a;
                    new_frame->argc = instr.c;

                    for (u32 i = 0; i < new_frame->argc; i++) {
                        new_frame->get_register(ARG_REGISTERS_OFFSET + i) =
                            current_frame->get_register(ARG_REGISTERS_OFFSET + i);
                    }

                    current_frame = std::move(new_frame);

                    if (mode == RunMode::StepIn) is_break = true;
                    break;
                }
                case Opcode::CallFf: {
                    Variant &func_var = current_frame->get_register(instr.b);

                    NativeFunction native_func = nullptr;

                    if (func_var.is_ptr()) {
                        native_func = reinterpret_cast<NativeFunction>(func_var.get_ptr());

                    } else if (func_var.is_int()) {
                        StringId sid = func_var.get_sid();
                        native_func = find_native_function(sid);

                        if (!native_func) {
                            lg::error("CallFf: native function '{}' (SID 0x{:016X}) not registered",
                                      sid.to_cstring(), static_cast<u64>(sid));
                            current_frame = nullptr;
                            is_error = true;
                            break_reason = "CallFf: unresolved native function";
                            break;
                        }
                    } else {
                        lg::error("CallFf: unexpected func type (not ptr, not int): {}",
                                  func_var.to_string());
                        current_frame = nullptr;
                        is_error = true;
                        break_reason = "CallFf: invalid func type";
                        break;
                    }

                    Variant &result_reg = current_frame->get_register(instr.a);
                    u32      argc = instr.c;

                    Variant *argv = &current_frame->get_register(ARG_REGISTERS_OFFSET);
                    result_reg = native_func(argc, argv);

                    // Cooperative yield / suspend check.
                    //
                    // A native function (e.g. wait-animate, sleep) calls
                    // Suspend() or RequestYield() on the current process.
                    // The VM must stop here, before the next instruction
                    // runs, and leave current_frame intact so the next
                    // tick resumes at the same pc.
                    if (Process *self = Carbon::current_process()) {
                        if (self->should_yield) {
                            self->should_yield = false;
                            is_yielded = true;
                            return result_reg;
                        }
                    }

                    if (mode == RunMode::StepIn) is_break = true;

                    break;
                }
                case Opcode::Branch: {
                    current_frame->pc = instr.imm16();
                    break;
                }

                case Opcode::BranchIf: {
                    Variant &condition = current_frame->get_register(instr.a);
                    if (condition.to_bool()) { current_frame->pc = instr.imm16(); }
                    break;
                }

                case Opcode::BranchIfNot: {
                    Variant &condition = current_frame->get_register(instr.a);
                    if (!condition.to_bool()) { current_frame->pc = instr.imm16(); }
                    break;
                }

                // ============================================================
                // Integer Arithmetic Instructions (0x1*)
                // ============================================================
                case Opcode::IAdd: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);

                    // Pointer arithmetic: if one operand is a pointer and
                    // the other is an integer, the result is a pointer
                    // shifted by that many bytes. This is what makes
                    // (at arr i) work: the base pointer is offset by
                    // i * elem_size, and the result must stay a Pointer
                    // for the following Load*/Store* to accept it.
                    if (src1.is_ptr() && src2.is_int()) {
                        auto *bytes = static_cast<std::byte *>(src1.get_ptr());
                        dest = Variant(static_cast<void *>(bytes + src2.to_int()),
                                       RuntimeType::Pointer);
                    } else if (src2.is_ptr() && src1.is_int()) {
                        auto *bytes = static_cast<std::byte *>(src2.get_ptr());
                        dest = Variant(static_cast<void *>(bytes + src1.to_int()),
                                       RuntimeType::Pointer);
                    } else {
                        dest = Variant(src1.to_int() + src2.to_int());
                    }
                    break;
                }

                case Opcode::ISub: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() - src2.to_int());
                    break;
                }

                case Opcode::IMul: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() * src2.to_int());
                    break;
                }

                case Opcode::IDiv: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    i32      divisor = src2.to_int();
                    if (divisor == 0) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src1.to_int() / divisor);
                    break;
                }

                case Opcode::IMod: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    i32      divisor = src2.to_int();
                    if (divisor == 0) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src1.to_int() % divisor);
                    break;
                }

                case Opcode::IAbs: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(std::abs(src.to_int()));
                    break;
                }

                case Opcode::INeg: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(-src.to_int());
                    break;
                }

                case Opcode::IntAsh: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    i32      value = src1.to_int();
                    i32      shift = src2.to_int();
                    dest = Variant(shift >= 0 ? value << shift : value >> -shift);
                    break;
                }

                case Opcode::CastInteger: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(src.to_int());
                    break;
                }

                // ============================================================
                // Integer Immediate Instructions (0x2*)
                // ============================================================
                case Opcode::LoadU16Imm: {
                    Variant &dest = current_frame->get_register(instr.a);
                    dest = Variant(instr.imm16());
                    break;
                }

                case Opcode::IAddImm: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    i32      imm = static_cast<i32>(instr.c);

                    // DC's IAddImm is used both for plain integer arithmetic and for
                    // pointer arithmetic (field address computation). In the pointer case
                    // the result must stay a Pointer; otherwise subsequent Load/Store
                    // opcodes fail with "expected pointer, got int".
                    if (src.is_ptr()) {
                        auto *bytes = static_cast<std::byte *>(src.get_ptr());
                        dest = Variant(static_cast<void *>(bytes + imm), RuntimeType::Pointer);
                    } else {
                        dest = Variant(src.to_int() + imm);
                    }
                    break;
                }
       

                case Opcode::ISubImm: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    i32      imm = static_cast<i32>(instr.c);

                    // DC's IAddImm is used both for plain integer arithmetic and for
                    // pointer arithmetic (field address computation). In the pointer case
                    // the result must stay a Pointer; otherwise subsequent Load/Store
                    // opcodes fail with "expected pointer, got int".
                    if (src.is_ptr()) {
                        auto *bytes = static_cast<std::byte *>(src.get_ptr());
                        dest = Variant(static_cast<void *>(bytes - imm), RuntimeType::Pointer);
                    } else {
                        dest = Variant(src.to_int() + imm);
                    }
                    break;
                }

                case Opcode::IMulImm: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    i32      imm = static_cast<i32>(instr.c);
                    dest = Variant(src.to_int() * imm);
                    break;
                }

                case Opcode::IDivImm: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    i32      imm = static_cast<i32>(instr.c);
                    if (imm == 0) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src.to_int() / imm);
                    break;
                }

                // ============================================================
                // Floating Point Instructions (0x3*)
                // ============================================================
                case Opcode::FAdd: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() + src2.to_float());
                    break;
                }

                case Opcode::FSub: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() - src2.to_float());
                    break;
                }

                case Opcode::FMul: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() * src2.to_float());
                    break;
                }

                case Opcode::FDiv: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    f32      divisor = src2.to_float();
                    if (divisor == 0.0f) {
                        lg::error("Division by zero");
                        break;
                    }
                    dest = Variant(src1.to_float() / divisor);
                    break;
                }

                case Opcode::FMod: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(std::fmod(src1.to_float(), src2.to_float()));
                    break;
                }

                case Opcode::FAbs: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(std::fabs(src.to_float()));
                    break;
                }

                case Opcode::FNeg: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(-src.to_float());
                    break;
                }

                case Opcode::CastFloat: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(src.to_float());
                    break;
                }

                // ============================================================
                // Comparison Instructions (0x4*)
                // ============================================================
                case Opcode::IEqual: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() == src2.to_int());
                    break;
                }
                // Integer not-equal.
                //
                // This opcode is emitted by NodeBuilder::build_compare
                // for the `!=` form. It is the counterpart of IEqual
                // and must be present in the switch, or the VM will
                // fall through to `default:` and abort with
                // "Unknown opcode: 91".
                case Opcode::INotEqual: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() != src2.to_int());
                    break;
                }

                case Opcode::IGreaterThan: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() > src2.to_int());
                    break;
                }

                case Opcode::IGreaterThanEqual: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() >= src2.to_int());
                    break;
                }

                case Opcode::ILessThan: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() < src2.to_int());
                    break;
                }

                case Opcode::ILessThanEqual: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() <= src2.to_int());
                    break;
                }

                case Opcode::FEqual: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(std::fabs(src1.to_float() - src2.to_float()) < 0.0001f);
                    break;
                }

                case Opcode::FGreaterThan: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() > src2.to_float());
                    break;
                }

                case Opcode::FGreaterThanEqual: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() >= src2.to_float());
                    break;
                }

                case Opcode::FLessThan: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() < src2.to_float());
                    break;
                }

                case Opcode::FLessThanEqual: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_float() <= src2.to_float());
                    break;
                }

                // ============================================================
                // Logical Instructions (0x5*)
                // ============================================================
                case Opcode::OpLogAnd: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_bool() && src2.to_bool());
                    break;
                }

                case Opcode::OpLogOr: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_bool() || src2.to_bool());
                    break;
                }

                case Opcode::OpLogNot: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(!src.to_bool());
                    break;
                }

                // ============================================================
                // Bitwise Instructions (0x6*)
                // ============================================================
                case Opcode::OpBitAnd: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() & src2.to_int());
                    break;
                }

                case Opcode::OpBitOr: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() | src2.to_int());
                    break;
                }

                case Opcode::OpBitXor: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(src1.to_int() ^ src2.to_int());
                    break;
                }

                case Opcode::OpBitNor: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src1 = current_frame->get_register(instr.b);
                    Variant &src2 = current_frame->get_register(instr.c);
                    dest = Variant(~(src1.to_int() | src2.to_int()));
                    break;
                }

                case Opcode::OpBitNot: {
                    Variant &dest = current_frame->get_register(instr.a);
                    Variant &src = current_frame->get_register(instr.b);
                    dest = Variant(~src.to_int());
                    break;
                }

                // ============================================================
                // Utility Instructions (0x7*)
                // ============================================================
                case Opcode::LoadParamCnt: {
                    Variant &dest = current_frame->get_register(instr.a);
                    dest = Variant(static_cast<i32>(current_frame->argc));
                    break;
                }

                // ============================================================
                // Lookup Instructions (0x8*)
                // ============================================================
                case Opcode::LookupInt: {
                    Variant &dest = current_frame->get_register(instr.a);
                    u32      idx = instr.b;
                    i64      value = current_frame->get_static_int(idx);
                    dest = Variant(value);
                    break;
                }

                case Opcode::LookupFloat: {
                    Variant &dest = current_frame->get_register(instr.a);
                    u32      idx = instr.b;
                    f64      value = current_frame->get_static_float(idx);
                    dest = Variant(value);
                    break;
                }
                case Opcode::LookupPointer: {
                    Variant &dest = current_frame->get_register(instr.a);
                    u32      idx = instr.b;

                    u64            raw = current_frame->get_static_int(idx);
                    const StringId sid = static_cast<StringId>(raw);

                    //fmt::print("; [LookupPointer] r{} <- SID '{}' (0x{:016X})\n",
                    //           static_cast<unsigned>(instr.a), sid.to_string(), raw);

                    // 1. Native function?
                    if (auto native = find_native_function(sid)) {
                        //fmt::print(";   -> native function {}\n", (void *)native);
                        dest = Variant(reinterpret_cast<void *>(native), RuntimeType::Pointer);
                        break;
                    }

                    // 2. Symbol registered in Globals?
                    if (auto *sym = carbon->globals()->find_symbol_ptr(sid)) {
                        //fmt::print(";   -> Globals slot {}\n", sym);
                        dest = Variant(sym, RuntimeType::Pointer);
                        break;
                    }

                    //fmt::print(";   -> NOT FOUND, returning SID as int\n");

                    // 3. Fallback: keep the SID as int.
                    dest = Variant(raw);
                    break;
                }


                // ============================================================
                // Indirect Load (через указатель)
                // ============================================================

                case Opcode::LoadInt:
                case Opcode::LoadI32:
                case Opcode::LoadU32: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i32   value = *reinterpret_cast<i32 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
                    break;
                }
                case Opcode::LoadFloat: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    f32   value = *reinterpret_cast<f32 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(static_cast<f64>(value));
                    break;
                }

                case Opcode::LoadPointer: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    void *value = *reinterpret_cast<void **>(ptr);
                    current_frame->get_register(instr.a) = Variant(value, RuntimeType::Pointer);
                    break;
                }
                // ============================================================
                // Indirect Load (разные размеры) — 0x73..0x7B
                // ============================================================
                case Opcode::LoadI8: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i8    value = *reinterpret_cast<i8 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
                    break;
                }
                case Opcode::LoadU8: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    u8    value = *reinterpret_cast<u8 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
                    break;
                }
                case Opcode::LoadI16: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i16   value = *reinterpret_cast<i16 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
                    break;
                }
                case Opcode::LoadU16: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    u16   value = *reinterpret_cast<u16 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
                    break;
                }

                case Opcode::LoadI64: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i64   value = *reinterpret_cast<i64 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(value);
                    break;
                }
                case Opcode::LoadU64: {
                    Variant &base = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    u64   value = *reinterpret_cast<u64 *>(ptr);
                    current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
                    break;
                }

                // ============================================================
                // Indirect Store Instructions (0xA*)
                // ============================================================
                case Opcode::StoreInt:
                case Opcode::StoreI32:
                case Opcode::StoreU32: {
                    Variant &regA = current_frame->get_register(instr.a);
                    Variant &regB = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, regA);
                    if (ptr == nullptr) break;
                    *reinterpret_cast<i32 *>(ptr) = regB.to_int();
                    break;
                }

                case Opcode::StoreFloat: {
                    Variant &regA = current_frame->get_register(instr.a);
                    Variant &regB = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, regA);
                    if (ptr == nullptr) break;
                    *reinterpret_cast<f32 *>(ptr) = regB.to_float();
                    break;
                }

                case Opcode::StorePointer: {
                    Variant &regA = current_frame->get_register(instr.a);
                    Variant &regB = current_frame->get_register(instr.b);
                    void    *ptr = checked_get_ptr(instr, instr.b, regA);
                    if (ptr == nullptr) break;
                    *reinterpret_cast<void **>(ptr) = regB.get_ptr();
                    break;
                }

                // ============================================================
                // Indirect Store (разные размеры) — 0x7C..0x8B
                //
                // Register convention: ALL Store* opcodes follow the canonical
                // VM rule, matching the documentation:
                //
                //     a = address
                //     b = value
                //
                // This is the same convention used by StoreInt, StoreI32,
                // StoreU32, StoreFloat, and StorePointer above. Do NOT swap
                // the operands for the narrower stores: they must use the
                // same order, or SetNode will write to the wrong address.
                // ============================================================
                case Opcode::StoreI8: {
                    auto  base = current_frame->get_register(instr.a);
                    void *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i64   value = current_frame->get_register(instr.b).get_i64();
                    *reinterpret_cast<i8 *>(ptr) = static_cast<i8>(value);
                    break;
                }
                case Opcode::StoreU8: {
                    auto  base = current_frame->get_register(instr.a);
                    void *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i64   value = current_frame->get_register(instr.b).get_i64();
                    *reinterpret_cast<u8 *>(ptr) = static_cast<u8>(value);
                    break;
                }
                case Opcode::StoreI16: {
                    auto  base = current_frame->get_register(instr.a);
                    void *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i64   value = current_frame->get_register(instr.b).get_i64();
                    *reinterpret_cast<i16 *>(ptr) = static_cast<i16>(value);
                    break;
                }
                case Opcode::StoreU16: {
                    auto  base = current_frame->get_register(instr.a);
                    void *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i64   value = current_frame->get_register(instr.b).get_i64();
                    *reinterpret_cast<u16 *>(ptr) = static_cast<u16>(value);
                    break;
                }
                case Opcode::StoreI64: {
                    auto  base = current_frame->get_register(instr.a);
                    void *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i64   value = current_frame->get_register(instr.b).get_i64();
                    *reinterpret_cast<i64 *>(ptr) = value;
                    break;
                }
                case Opcode::StoreU64: {
                    auto base = current_frame->get_register(instr.a);
                    void *ptr = checked_get_ptr(instr, instr.b, base);
                    if (ptr == nullptr) break;
                    i64   value = current_frame->get_register(instr.b).get_i64();
                    *reinterpret_cast<u64 *>(ptr) = static_cast<u64>(value);
                    break;
                }
                    // ============================================================
                    // Indirect Load Instructions
                    // ------------------------------------------------------------
                    // В этих командах номер константы в регистре а не в imediate operand
                    // ============================================================

                case Opcode::LoadStaticInt: {
                    u32 index = instr.b; // ← индекс, не регистр
                    i64 value = current_frame->get_static_int(index);
                    current_frame->get_register(instr.a) = Variant(value);
                    break;
                }

                case Opcode::LoadStaticFloat: {
                    u32 index = instr.b; // ← индекс
                    f64 value = current_frame->get_static_float(index);
                    current_frame->get_register(instr.a) = Variant(value);
                    break;
                }

                case Opcode::LoadStaticPointer: {
                    u32   index = instr.b; // ← индекс
                    void *ptr = current_frame->get_static_pointer(index);
                    current_frame->get_register(instr.a) = Variant(ptr, RuntimeType::Pointer);
                    break;
                }
                // ============================================================
                // Static Load Instructions
                // ------------------------------------------------------------
                // Загружает константу из SYMBOL TABLE
                // ============================================================
                case Opcode::LoadStaticI8Imm:
                case Opcode::LoadStaticU8Imm:
                case Opcode::LoadStaticI16Imm:
                case Opcode::LoadStaticU16Imm:
                case Opcode::LoadStaticI32Imm:
                case Opcode::LoadStaticU32Imm:
                case Opcode::LoadStaticI64Imm:
                case Opcode::LoadStaticU64Imm:
                case Opcode::LoadStaticFloatImm:
                case Opcode::LoadStaticPointerImm: {
                    // Для этих инструкций индекс — это НЕ imm16, а поле b (operand1)!
                    u32 index = instr.b; // ← байт 2 = operand1 = индекс в ST!

                    if (instr.opcode == Opcode::LoadStaticFloatImm) {
                        f64 value = current_frame->get_static_float(index);
                        current_frame->get_register(instr.a) = Variant(value);
                    } else if (instr.opcode == Opcode::LoadStaticPointerImm) {
                        void *ptr = current_frame->get_static_pointer(index);
                        current_frame->get_register(instr.a) = Variant(ptr, RuntimeType::Pointer);
                    } else {
                        i64 value = current_frame->get_static_int(index);
                        current_frame->get_register(instr.a) = Variant(value);
                    }
                    break;
                }
                case Opcode::StoreArray: {
                    // _RAX = _RDI[_R15]  -> src_ptr
                    // _RCX = _RDI[_RSI]  -> dst_ptr
                    // _RDI[_RBP] = _RDI[_R15] -> результат = src_ptr

                    void *src = current_frame->get_register(instr.c).get_ptr(); // _R15
                    void *dst = current_frame->get_register(instr.b).get_ptr(); // _RSI

                    // Копируем 32 байта (размер YMM регистра)
                    std::memcpy(dst, src, 32);

                    // Результат = src
                    current_frame->get_register(instr.a) = Variant(src, RuntimeType::Pointer);
                    break;
                }
                // ============================================================
                // Default case for unknown opcodes
                // ============================================================
                default: {
                    lg::error("Unknown opcode: {} (0x{:02X})", static_cast<u32>(instr.opcode),
                              static_cast<u32>(instr.opcode));
                    current_frame = nullptr;
                    break;
                }
                } // end of case
            } catch (const VmError &e) {
                // A C++-thrown VmError. Record it and stop.
                set_error(e);
                // The while loop will check m_has_error next iteration
                // and return.
                break; // out of the switch
            } catch (const TypeError &e) {
                // Variant::get_ptr, to_int, etc.
                set_error(VmError::Kind::TypeMismatch, e.what(), &instr);
                break;
            } catch (const std::exception &e) {
                // Anything else that escaped from a native function.
                set_error(VmError::Kind::NativeError, e.what(), &instr);
                break;
            } catch (...) {
                set_error(VmError::Kind::Internal, "unknown exception", &instr);
                break;
            }
            if (mode == RunMode::Step) { is_break = true; }
        } // end of loop

        return final_result;
    }

    void VirtualMachine::set_error(VmError::Kind kind, const std::string &message,
                                   const Instruction *instr) {
        m_pending_error = VmError(kind, message, current_frame, instr);
        m_has_error = true;
    }

    void VirtualMachine::set_error(VmError err) {
        m_pending_error = std::move(err);
        m_has_error = true;
    }

    // Read the pointer stored in register `regnum` of the current frame
    // and return it through `out_ptr`.
    //
    // On success:
    //   * `out_ptr` is set to a non-null pointer.
    //   * returns false.
    //
    // On failure:
    //   * an error is recorded via set_error();
    //   * returns true.
    //
    // The caller is expected to do:
    //     void *ptr;
    //     if (checked_get_ptr(instr, instr.b, base, "LoadFloat", ptr)) break;
    //     ... use ptr ...
    //
    // `op_name` is used in the error message so that reports point at
    // the actual opcode, not a hardcoded one.
    [[nodiscard]] void *VirtualMachine::checked_get_ptr(Instruction &instr, u8 regnum, Variant &reg) {
        if (!reg.is_ptr()) {
            set_error(VmError::Kind::TypeMismatch,
                      fmt::format("{}: register r{} is not a pointer (got {})",
                                  opcode_mnemonic(instr.opcode),
                                  static_cast<unsigned>(regnum),
                                  value_type_to_string(reg.get_type())),
                      &instr);
            return nullptr;
        }

        void *ptr = reg.get_ptr();
        if (ptr == nullptr) {
            set_error(
                VmError::Kind::NullDeref,
                      fmt::format("{}: null base pointer in r{}", 
                          opcode_mnemonic(instr.opcode), 
                          static_cast<unsigned>(regnum)),
                &instr);
            return nullptr;
        }

        return ptr;
    }
} // namespace carbon