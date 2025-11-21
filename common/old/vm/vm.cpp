#include <cmath>
#include <iostream>
#include <variant>
#include "string_id.hpp"
#include "vm.hpp"

#include "assembler_opcodes.hpp"
#include "module_bin.hpp"
#include "context.hpp"
#include "environment.hpp"
#include "math.hpp"
#include "module.hpp"
#include "native_func.hpp"

namespace vm {

std::vector<FStackFrame*> g_task_list;

inline FVariant& GetregA(FStackFrame* frame, FInstr instr) {
  const u32 regIdx = instr.a;
  return frame->get_register(regIdx);
}

inline FVariant& GetregB(FStackFrame* frame, FInstr instr) {
  const u32 regIdx = instr.b;
  return frame->get_register(regIdx);
}

inline FVariant& GetregC(FStackFrame* frame, FInstr instr) {
  const u32 regIdx = instr.c;
  return frame->get_register(regIdx);
}

FStackFrame* PushStackFrame(FByteCode* pCode, FStackFrame* pParent) {
  auto* pFrame = new FStackFrame();
  pFrame->parent_ptr = pParent;
  pFrame->code_ptr = pCode->get_code_ptr();
  pFrame->data_ptr = pCode->get_data_ptr();
  return pFrame;
}

FStackFrame* PopStackFrame(FStackFrame* pFrame) {
  const auto pParent = pFrame->parent_ptr;
  delete (pFrame);
  return pParent;
}

FInstr FStackFrame::GetNextInstruction() {
  return code_ptr[pc++];
}

FStackFrame* ExecuteScript(FStackFrame* frame, FLocalEnvironment& env) {
  while (frame != nullptr) {
    FInstr instr = frame->GetNextInstruction();
#ifdef DEBUG_PRINT_EXECUTED_INSTRUCTION
    std::cout << "Step PC=" << frame->PC << std::endl;
    print_registers(frame->Registers, DC_FRAME_MAX_REGISTERS_NUM);
    std::cout << assembler::decompile_instruction(instr, frame->DataPtr) << std::endl;
#endif
    switch (instr.opcode) {
      case EOpcode::Return: {
        FVariant regA = GetregA(frame, instr);
        auto retNum = frame->ret_num;
        frame = PopStackFrame(frame);
        if (frame != nullptr)
          frame->registers[retNum] = regA;
      } break;
      case EOpcode::Call: {
        FVariant& regA = GetregA(frame, instr);  // Function name
        if (!regA.is_null()) {
          FByteCode* code = reinterpret_cast<FByteCode*>(regA.get_as_ptr());
          auto oldFrame = frame;
          frame = PushStackFrame(code, frame);
          frame->ret_num = instr.b;  // Return value to this register
          frame->argc = instr.c;     // value of arguments
          for (size_t i = 0; i < frame->argc; i++)
            frame->registers[ARGUMENT_REGISTERS_OFFSET + i] =
                oldFrame->registers[ARGUMENT_REGISTERS_OFFSET + i];
        } else {
          throw FRuntimeError(g_script_context, "null pointer call");
        }
      } break;
      case EOpcode::CallNat: {
        FVariant& regA = GetregA(frame, instr);  // Function name
        if (regA.is_null())
          throw FRuntimeError(g_script_context, "null pointer call native");
        if (regA.type != SID("native"))
          throw FRuntimeError(g_script_context, "call-native expect 'native' pointer but found",
                              regA.to_string());
        const auto func = reinterpret_cast<FNativeFunction*>(regA.get_as_ptr());
        FVariant& regB = GetregB(frame, instr);  // Function name
        regB = func(instr.c, &frame->registers[ARGUMENT_REGISTERS_OFFSET]);
      } break;
      case EOpcode::Branch: {
        frame->pc = instr.k;
      } break;
      case EOpcode::BranchIf: {
        FVariant& regA = GetregA(frame, instr);
        if (regA.get_as_bool())
          frame->pc = instr.k;
      } break;
      case EOpcode::BranchIfNot: {
        FVariant& regA = GetregA(frame, instr);
        if (!regA.get_as_bool())
          frame->pc = instr.k;
      } break;
      case EOpcode::Move: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        regA = regB;
      } break;
      /**
       * Integer comparisongs
       */
      case EOpcode::CmpEqual: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() == regC.get_as_s32();
      } break;
      case EOpcode::CmpGt: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() > regC.get_as_s32();
      } break;
      case EOpcode::CmpGtEqual: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() >= regC.get_as_s32();
      } break;
      case EOpcode::CmpLt: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() < regC.get_as_s32();
      } break;
      case EOpcode::CmpLtEqual: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() <= regC.get_as_s32();
      } break;
        /**
         * Floating point comparisongs
         */
      case EOpcode::CmpFloatEqual: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = compare_float(regB.get_as_f32(), regC.get_as_f32());
      } break;
      case EOpcode::CmpFloatGt: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() > regC.get_as_f32();
      } break;
      case EOpcode::CmpFloatGtEqual: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() >= regC.get_as_f32();
      } break;
      case EOpcode::CmpFloatLt: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() < regC.get_as_f32();
      } break;
      case EOpcode::CmpFloatLtEqual: {
        FVariant& regA = GetregA(frame, instr);
        FVariant& regB = GetregB(frame, instr);
        FVariant& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() <= regC.get_as_f32();
      } break;
        /**
         * Load immediate integer value for different types
         */
      case EOpcode::LoadImediateInt: {
        auto imm = instr.k;
        FVariant& regA = GetregA(frame, instr);
        regA = imm;
      } break;
        /**
         * Load istatic value for different types
         */
      case EOpcode::LoadIndInt: {
        FVariant& regB = GetregB(frame, instr);
        if (regB.is_ptr()) {
          FVariant& regA = GetregA(frame, instr);
          s32 val = *reinterpret_cast<s32*>(regB.get_as_ptr());
          regA = val;
        } else
          throw FRuntimeError(g_script_context, "Expected a pointer", regB.to_c_string());
      } break;

      case EOpcode::LoadIndFloat: {
        FVariant& regB = GetregB(frame, instr);
        if (regB.is_ptr()) {
          FVariant& regA = GetregA(frame, instr);
          float val = *reinterpret_cast<float*>(regB.get_as_ptr());
          regA = val;
        } else
          throw FRuntimeError(g_script_context, "Expected a pointer", regB.to_c_string());
      } break;

      case EOpcode::LoadIndPointer: {
        FVariant& regB = GetregB(frame, instr);
        if (regB.is_ptr()) {
          FVariant& regA = GetregA(frame, instr);
          PTRINT val = *reinterpret_cast<PTRINT*>(regB.get_as_ptr());
          regA = val;
        } else
          throw FRuntimeError(g_script_context, "Expected a pointer", regB.to_c_string());
      } break;
      /**
       * Load istatic value for different types
       */
      case EOpcode::StoreIndInt: {
        FVariant& regA = GetregB(frame, instr);
        if (regA.is_ptr()) {
          FVariant& regB = GetregB(frame, instr);
          *reinterpret_cast<s32*>(regA.get_as_ptr()) = regB.get_as_s32();
        } else
          throw FRuntimeError(g_script_context, "Expected a pointer", regA.to_c_string());
      } break;

      case EOpcode::StoreIndFloat: {
        FVariant& regA = GetregB(frame, instr);
        if (regA.is_ptr()) {
          FVariant& regB = GetregB(frame, instr);
          *reinterpret_cast<float*>(regA.get_as_ptr()) = regB.get_as_f32();
        } else
          throw FRuntimeError(g_script_context, "Expected a pointer", regA.to_c_string());
      } break;

      case EOpcode::StoreIndPointer: {
        FVariant& regA = GetregB(frame, instr);
        if (regA.is_ptr()) {
          FVariant& regB = GetregB(frame, instr);
          *reinterpret_cast<PTRINT*>(regA.get_as_ptr()) = regB.get_as_ptr();
        } else
          throw FRuntimeError(g_script_context, "Expected a pointer", regA.to_c_string());
      } break;
      /**
       * Load istatic value for different types
       */
      case EOpcode::LoadStaticInt: {
        FVariant& regA = GetregA(frame, instr);
        regA = frame->get_static_s32(instr.k);
      } break;

      case EOpcode::LoadStaticFloat: {
        auto& regA = GetregA(frame, instr);
        regA = frame->get_static_f32(instr.k);
      } break;

      case EOpcode::LoadStaticPointer: {
        auto& regA = GetregA(frame, instr);
        regA = frame->get_static_pointer(instr.k);
      } break;

      case EOpcode::LookupInt: {
        auto& regA = GetregA(frame, instr);
        auto imm = instr.k;
        auto name = frame->get_static_s32(imm);
        auto def = env.lookup(name, SID("s32"), true);
        if (def == nullptr)
          throw FRuntimeError(g_script_context, "Definition is not found",
                              lookup_string_safe(name));
        regA = *((s32*)def->ptr);
      } break;
      case EOpcode::LookupFloat: {
        auto& regA = GetregA(frame, instr);
        auto imm = instr.k;
        auto name = frame->get_static_s32(imm);
        auto def = env.lookup(name, SID("float"), true);
        if (def == nullptr)
          throw FRuntimeError(g_script_context, "Definition is not found",
                              lookup_string_safe(name));
        regA = *((float*)def->ptr);
      } break;
      case EOpcode::LookupPointer: {
        auto& regA = GetregA(frame, instr);
        auto imm = instr.k;
        auto name = frame->get_static_s32(imm);
        auto def = env.lookup(name, true);
        if (def == nullptr)
          throw FRuntimeError(g_script_context, "Definition is not found",
                              lookup_string_safe(name));
        regA = def->ptr;
      } break;
      /**
       * Add,Sub,Mul,Div,Mod,Abs integer values
       */
      case EOpcode::AddInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() + regC.get_as_s32();
      } break;
      case EOpcode::SubInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() - regC.get_as_s32();
      } break;
      case EOpcode::MulInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() * regC.get_as_s32();
      } break;
      case EOpcode::DivInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() / regC.get_as_s32();
      } break;
      case EOpcode::ModInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() % regC.get_as_s32();
      } break;
      case EOpcode::AbsInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = abs(regB.get_as_s32());
      } break;
      case EOpcode::NegInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = -regB.get_as_s32();
      } break;
      case EOpcode::AshInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto value = regA.get_as_s32();
        auto shift = regB.get_as_s32();
        regA = shift >= 0 ? value << shift : value >> -shift;
      } break;
      case EOpcode::ToInt: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = regB.cast_to_s32();
      } break;
      /**
       * Add,Sub,Mul,Div,Mod,Abs floats
       */
      case EOpcode::AddFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() + regC.get_as_f32();
      } break;
      case EOpcode::SubFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() - regC.get_as_f32();
      } break;
      case EOpcode::MulFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() * regC.get_as_f32();
      } break;
      case EOpcode::DivFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_f32() / regC.get_as_f32();
      } break;
      case EOpcode::ModFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = fmod(regB.get_as_f32(), regC.get_as_f32());
      } break;
      case EOpcode::AbsFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = fabs(regB.get_as_f32());
      } break;
      case EOpcode::NegFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        float A = regB.get_as_f32();
        regA = -A;
      } break;
      case EOpcode::ToFloat: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = regB.cast_to_f32();
      } break;
      /**
       * Add,Sub,Mul,Div integere immediate value
       */
      case EOpcode::AddImm: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto imm = (s32)instr.c;
        regA = regB.get_as_s32() + imm;
      } break;

      case EOpcode::SubImm: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto imm = (s32)instr.c;
        regA = regB.get_as_s32() - imm;
      } break;

      case EOpcode::MulImm: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto imm = (s32)instr.c;
        regA = regB.get_as_s32() * imm;
      } break;

      case EOpcode::DivImm: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto imm = (s32)instr.c;
        regA = regB.get_as_s32() / imm;
      } break;

      /**
       * The logical operations
       */
      case EOpcode::LogAnd: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() != 0 && regC.get_as_s32() != 0;
      } break;

      case EOpcode::LogOr: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() != 0 || regC.get_as_s32() != 0;
      } break;

      case EOpcode::LogNot: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = regB.get_as_s32() == 0;
      } break;
#ifdef USE_BIT_OPERATIONS
      /**
       * The bit operations are mostly unused in the game developing
       * they can be ommited for the low footprint
       */
      case EOpcode::BitAnd: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() & regC.get_as_s32();
      } break;

      case EOpcode::BitOr: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() || regC.get_as_s32();
      } break;

      case EOpcode::BitXor: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = regB.get_as_s32() ^ regC.get_as_s32();
      } break;

      case EOpcode::BitNor: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        auto& regC = GetregC(frame, instr);
        regA = ~(regB.get_as_s32() | regC.get_as_s32());
      } break;

      case EOpcode::BitNot: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = ~regB.get_as_s32();
      } break;
#endif

      case EOpcode::LocadArgc: {
        auto& regA = GetregA(frame, instr);
        regA = frame->get_argc();
      } break;

      case EOpcode::GetSidStr: {
        auto& regA = GetregA(frame, instr);
        auto& regB = GetregB(frame, instr);
        regA = reinterpret_cast<PTRINT>(lookup_string_safe(regB.get_as_s32()).c_str());
        break;
      }
    }
  }
  // The function complete
  return nullptr;
}

/**
 * @brief Execute the virtual machine code
 * @param pCode - the code pointer
 * @param pFrame - the frame pointer or null
 * @param env - the environment to run
 * @attention In any case will be constructed new frame
 */
FStackFrame* ExecuteScript(FByteCode* pCode, FStackFrame* pFrame, FLocalEnvironment& env) {
  const auto pNewFrame = PushStackFrame(pCode, pFrame);
  return ExecuteScript(pNewFrame, env);
}

std::string FStackFrame::to_string() const {
  return std::format("{0} {1} {2} {3}", to_str((PTRINT)code_ptr), to_str(parent_ptr), to_str(pc),
                     to_str(argc));
}

/**
 * @brief Execute the virtual machine code
 * @param name - the function name
 * @attention In any case will be constructed new frame
 */
FStackFrame* ExecuteScript(const StringId name) {
  const auto def = g_environment.lookup(name);
  if (def == nullptr || !def->is_valid())
    throw FRuntimeError(g_script_context, "Definition not found", to_str(name));
  if (def->type != SID("lambda"))
    throw FTypeError(g_script_context, name, SID("lambda"), def->type);
  auto& env = def->module_ref->module_ref->env;
  const auto pNewFrame = PushStackFrame((FByteCode*)def->ptr, nullptr);
  return ExecuteScript(pNewFrame, env);
}

std::string to_str(FStackFrame* obj) {
  if (obj == nullptr)
    return "#FStackFrame <null>";
  return obj->to_string();
}

}  // namespace vm
