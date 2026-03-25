#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/files/BinaryFile.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/util/Log.hpp"
#include "lib/StringId.hpp"
#include "lib/Variant.hpp"

using namespace carbon::lib;
using namespace carbon::files;
using namespace carbon::modules;


namespace carbon::vm {

	// ------------------------------------------------------------------------
	// Internal Helpers
	// ------------------------------------------------------------------------

	vm_int VirtualMachine::resolve_integer(std::shared_ptr<StackFrame> frame, StringId name) {
		auto module = frame->byte_code->owner_module;
		if (module) {
			Definition* resolved = module->resolve_symbol(name);
			if (resolved) {
				if (resolved->type == TypeIds::i64)
					return *((s64*)resolved->data.get());
				if (resolved->type == TypeIds::i32)
					return *((s32*)resolved->data.get());
				if (resolved->type == TypeIds::i16)
					return *((s16*)resolved->data.get());
				if (resolved->type == TypeIds::i8)
					return *((s8*)resolved->data.get());

				if (resolved->type == TypeIds::u64)
					return *((u64*)resolved->data.get());
				if (resolved->type == TypeIds::u32)
					return *((u32*)resolved->data.get());
				if (resolved->type == TypeIds::u16)
					return *((u16*)resolved->data.get());
				if (resolved->type == TypeIds::u8)
					return *((u8*)resolved->data.get());

            	throw VmTypeError("resolve_integer", frame, resolved->type);
			}
			else {
				throw VmResolvingError("resolve_integer", frame, name);
			}
		}
		throw VmResolvingError("resolve_integer", frame, name);
	}

	vm_float VirtualMachine::resolve_float(std::shared_ptr<StackFrame> frame, StringId name) {
		auto module = frame->byte_code->owner_module;
		if (module) {
			Definition* resolved = module->resolve_symbol(name);
			if (resolved) {
				if (resolved->type == TypeIds::i64)
					return *((s64*)resolved->data.get());
				if (resolved->type == TypeIds::i32)
					return *((s32*)resolved->data.get());
				if (resolved->type == TypeIds::i16)
					return *((s16*)resolved->data.get());
				if (resolved->type == TypeIds::i8)
					return *((s8*)resolved->data.get());

				if (resolved->type == TypeIds::u64)
					return *((u64*)resolved->data.get());
				if (resolved->type == TypeIds::u32)
					return *((u32*)resolved->data.get());
				if (resolved->type == TypeIds::u16)
					return *((u16*)resolved->data.get());
				if (resolved->type == TypeIds::u8)
					return *((u8*)resolved->data.get());

            	throw VmTypeError("resolve_float", frame, resolved->type);
			}
			else {
				throw VmResolvingError("resolve_float", frame, name);
			}
		}
		throw VmResolvingError("resolve_float", frame, name);
	}

	void* VirtualMachine::resolve_pointer(std::shared_ptr<StackFrame> frame, StringId name) {
		auto module = frame->byte_code->owner_module;
		if (module) {
			Definition* resolved = module->resolve_symbol(name);
			if (resolved) {
				return resolved->data.get();
			}
			else {
				throw VmResolvingError("resolve_pointer", frame, name);
			}
		}
		throw VmResolvingError("resolve_pointer", frame, name);
	}

	// ------------------------------------------------------------------------
	// Process Management (НОВОЕ - согласно нашему базису)
	// ------------------------------------------------------------------------

	// Исполнение функции в модуле
	Variant VirtualMachine::execute_function(Module* module, StringId function, RunMode mode)
	{
		FunctionDesc* code = module->resolve_function(function);

		if (code) {
			return execute_function(code, mode);
		}
		else {
			lg::warn("VM: No main code found for process {}", module->name, function);
		}

		return Variant();
	}

	// Исполнение кода 
	Variant VirtualMachine::execute_function(FunctionDesc* FunctionDesc, RunMode mode) {
		if (!FunctionDesc) {
			lg::error("Cannot execute null FunctionDesc");
			return Variant();
		}

		auto current_frame = create_stack_frame(
			FunctionDesc->get_code_ptr(),
			FunctionDesc->get_data_ptr(),
			nullptr
		);
		return execute(current_frame, mode);
	}

	Variant VirtualMachine::execute(std::shared_ptr<StackFrame> stack_frame, RunMode mode) {
		current_frame = stack_frame;
		return execute(mode);
	}

	// Исполнение кода связанного со стек фреймом
	Variant VirtualMachine::execute(RunMode mode) {
		Variant final_result;
		
		// Исполнение кода
		while (current_frame != nullptr) {
			if (is_break)
				return Variant();

			Instruction instr = current_frame->get_next_instruction();

			if (enable_debug_log)
				lg::debug("PC={} : {}", current_frame->pc - 1, InstructionTable::instance().disassemble(instr));

			try {

				// Исколнение конкретной инструкции
				switch (instr.opcode) {
					// ============================================================
					// Control Flow Instructions (0x0*)
					// ============================================================
					case Opcode::RETURN: {
						Variant return_value = current_frame->get_register(instr.a);
						
						// Получаем shared_ptr родителя через lock()
						auto parent_frame = current_frame->get_parent();  // возвращает shared_ptr
						
						if (!parent_frame) {
							final_result = return_value;
							current_frame.reset();  // освобождаем текущий фрейм
						}
						else {
							parent_frame->get_register(current_frame->ret_num) = return_value;
							
							// Перемещаем current_frame на родителя
							current_frame = parent_frame;  // shared_ptr присваивание
							// Старый current_frame автоматически удалится, если на него нет других ссылок
						}
						
						if (mode == RunMode::StepOut)
							is_break = true;
						break;
					}

					case Opcode::MOVE: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = src;
						break;
					}

					case Opcode::CALL: {
						Variant& func_var = current_frame->get_register(instr.a);

						if (!func_var.is_function()) {
							lg::error("Call target is not a lambda: {}", func_var.to_string());
							current_frame = nullptr;
							break;
						}

						FunctionDesc* target_code = reinterpret_cast<FunctionDesc*>(func_var.get_ptr());
						auto new_frame = create_stack_frame(
							target_code->get_code_ptr(),
							target_code->get_data_ptr(),
							current_frame
						);

						new_frame->ret_num = instr.b;
						new_frame->argc = instr.c;

						for (u32 i = 0; i < new_frame->argc; i++) {
							new_frame->get_register(ARG_REGISTERS_OFFSET + i) =
								current_frame->get_register(ARG_REGISTERS_OFFSET + i);
						}

						current_frame = std::move(new_frame);

						if (mode == RunMode::StepIn)
							is_break = true;
						break;
					}

					case Opcode::CALL_NATIVE: {
						Variant& func_var = current_frame->get_register(instr.a);
						NativeFunction native_func = nullptr;

						if (func_var.is_ptr() && func_var.get_type() == SID("native")) {
							native_func = reinterpret_cast<NativeFunction>(func_var.get_ptr());
						}
						else if (func_var.is_sid()) {
							native_func = find_native_function(func_var.get_sid());
						}

						if (!native_func) {
							lg::error("Native function not found: {}", func_var.to_string());
							break;
						}

						Variant& result_reg = current_frame->get_register(instr.b);
						u32 argc = instr.c;

						Variant* argv = &current_frame->get_register(ARG_REGISTERS_OFFSET);
						result_reg = native_func(argc, argv);
						break;
					}

					case Opcode::BRANCH: {
						current_frame->pc = instr.imm16;
						break;
					}

					case Opcode::BRANCH_IF: {
						Variant& condition = current_frame->get_register(instr.a);
						if (condition.to_bool()) {
							current_frame->pc = instr.imm16;
						}
						break;
					}

					case Opcode::BRANCH_IF_NOT: {
						Variant& condition = current_frame->get_register(instr.a);
						if (!condition.to_bool()) {
							current_frame->pc = instr.imm16;
						}
						break;
					}

					// ============================================================
					// Integer Arithmetic Instructions (0x1*)
					// ============================================================
					case Opcode::ADD_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() + src2.to_int());
						break;
					}

					case Opcode::SUB_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() - src2.to_int());
						break;
					}

					case Opcode::MUL_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() * src2.to_int());
						break;
					}

					case Opcode::DIV_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						s32 divisor = src2.to_int();
						if (divisor == 0) {
							lg::error("Division by zero");
							break;
						}
						dest = Variant(src1.to_int() / divisor);
						break;
					}

					case Opcode::MOD_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						s32 divisor = src2.to_int();
						if (divisor == 0) {
							lg::error("Division by zero");
							break;
						}
						dest = Variant(src1.to_int() % divisor);
						break;
					}

					case Opcode::ABS_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(std::abs(src.to_int()));
						break;
					}

					case Opcode::NEG_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(-src.to_int());
						break;
					}

					case Opcode::ASH_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						s32 value = src1.to_int();
						s32 shift = src2.to_int();
						dest = Variant(shift >= 0 ? value << shift : value >> -shift);
						break;
					}

					case Opcode::TO_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(src.to_int());
						break;
					}

					// ============================================================
					// Integer Immediate Instructions (0x2*)
					// ============================================================
					case Opcode::LOAD_IMMEDIATE_INT: {
						Variant& dest = current_frame->get_register(instr.a_imm);
						dest = Variant(static_cast<s32>(instr.imm16));
						break;
					}

					case Opcode::ADD_IMM: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						s32 imm = static_cast<s32>(instr.c);
						dest = Variant(src.to_int() + imm);
						break;
					}

					case Opcode::SUB_IMM: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						s32 imm = static_cast<s32>(instr.c);
						dest = Variant(src.to_int() - imm);
						break;
					}

					case Opcode::MUL_IMM: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						s32 imm = static_cast<s32>(instr.c);
						dest = Variant(src.to_int() * imm);
						break;
					}

					case Opcode::DIV_IMM: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						s32 imm = static_cast<s32>(instr.c);
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
					case Opcode::ADD_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() + src2.to_float());
						break;
					}

					case Opcode::SUB_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() - src2.to_float());
						break;
					}

					case Opcode::MUL_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() * src2.to_float());
						break;
					}

					case Opcode::DIV_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						f32 divisor = src2.to_float();
						if (divisor == 0.0f) {
							lg::error("Division by zero");
							break;
						}
						dest = Variant(src1.to_float() / divisor);
						break;
					}

					case Opcode::MOD_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(std::fmod(src1.to_float(), src2.to_float()));
						break;
					}

					case Opcode::ABS_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(std::fabs(src.to_float()));
						break;
					}

					case Opcode::NEG_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(-src.to_float());
						break;
					}

					case Opcode::TO_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(src.to_float());
						break;
					}

					// ============================================================
					// Comparison Instructions (0x4*)
					// ============================================================
					case Opcode::CMP_EQUAL: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() == src2.to_int());
						break;
					}

					case Opcode::CMP_GT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() > src2.to_int());
						break;
					}

					case Opcode::CMP_GT_EQUAL: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() >= src2.to_int());
						break;
					}

					case Opcode::CMP_LT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() < src2.to_int());
						break;
					}

					case Opcode::CMP_LT_EQUAL: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() <= src2.to_int());
						break;
					}

					case Opcode::CMP_FLOAT_EQUAL: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(std::fabs(src1.to_float() - src2.to_float()) < 0.0001f);
						break;
					}

					case Opcode::CMP_FLOAT_GT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() > src2.to_float());
						break;
					}

					case Opcode::CMP_FLOAT_GT_EQUAL: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() >= src2.to_float());
						break;
					}

					case Opcode::CMP_FLOAT_LT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() < src2.to_float());
						break;
					}

					case Opcode::CMP_FLOAT_LT_EQUAL: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() <= src2.to_float());
						break;
					}

					// ============================================================
					// Logical Instructions (0x5*)
					// ============================================================
					case Opcode::LOG_AND: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_bool() && src2.to_bool());
						break;
					}

					case Opcode::LOG_OR: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_bool() || src2.to_bool());
						break;
					}

					case Opcode::LOG_NOT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(!src.to_bool());
						break;
					}

					// ============================================================
					// Bitwise Instructions (0x6*)
					// ============================================================
					case Opcode::BIT_AND: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() & src2.to_int());
						break;
					}

					case Opcode::BIT_OR: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() | src2.to_int());
						break;
					}

					case Opcode::BIT_XOR: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() ^ src2.to_int());
						break;
					}

					case Opcode::BIT_NOR: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(~(src1.to_int() | src2.to_int()));
						break;
					}

					case Opcode::BIT_NOT: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(~src.to_int());
						break;
					}

					// ============================================================
					// Utility Instructions (0x7*)
					// ============================================================
					case Opcode::LOAD_ARGC: {
						Variant& dest = current_frame->get_register(instr.a);
						dest = Variant(static_cast<s32>(current_frame->argc));
						break;
					}

					case Opcode::GET_SID_STRING: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(src.get_sid().to_string());
						break;
					}

					// ============================================================
					// Lookup Instructions (0x8*)
					// ============================================================
					case Opcode::LOOKUP_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						auto name_id = StringId(current_frame->get_static_int(instr.imm16));
						dest = Variant(resolve_integer(current_frame, name_id));
						break;
					}

					case Opcode::LOOKUP_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						auto name_id = StringId(current_frame->get_static_int(instr.imm16));
						dest = Variant(resolve_float(current_frame, name_id));
						break;
					}

					case Opcode::LOOKUP_POINTER: {
						Variant& dest = current_frame->get_register(instr.a);
						auto name_id = StringId(current_frame->get_static_int(instr.imm16));
						dest = Variant(resolve_pointer(current_frame, name_id));
						break;
					}

					// ============================================================
					// Indirect Load Instructions (0x9*)
					// ============================================================
					case Opcode::LOAD_IND_INT: {
						Variant& regB = current_frame->get_register(instr.b);
						if (regB.is_ptr()) {
							Variant& regA = current_frame->get_register(instr.a);
							s32 val = *reinterpret_cast<s32*>(regB.get_ptr());
							regA = Variant(val);
						}
						else {
							lg::error("LOAD_IND_INT: Expected pointer, got {}", regB.to_string());
						}
						break;
					}

					case Opcode::LOAD_IND_FLOAT: {
						Variant& regB = current_frame->get_register(instr.b);
						if (regB.is_ptr()) {
							Variant& regA = current_frame->get_register(instr.a);
							f32 val = *reinterpret_cast<f32*>(regB.get_ptr());
							regA = Variant(val);
						}
						else {
							lg::error("LOAD_IND_FLOAT: Expected pointer, got {}", regB.to_string());
						}
						break;
					}

					case Opcode::LOAD_IND_POINTER: {
						Variant& regB = current_frame->get_register(instr.b);
						if (regB.is_ptr()) {
							Variant& regA = current_frame->get_register(instr.a);
							void* val = *reinterpret_cast<void**>(regB.get_ptr());
							regA = Variant(val);
						}
						else {
							lg::error("LOAD_IND_POINTER: Expected pointer, got {}", regB.to_string());
						}
						break;
					}

					// ============================================================
					// Indirect Store Instructions (0xA*)
					// ============================================================
					case Opcode::STORE_IND_INT: {
						Variant& regA = current_frame->get_register(instr.a);
						Variant& regB = current_frame->get_register(instr.b);
						if (regA.is_ptr()) {
							*reinterpret_cast<s32*>(regA.get_ptr()) = regB.to_int();
						}
						else {
							lg::error("STORE_IND_INT: Expected pointer, got {}", regA.to_string());
						}
						break;
					}

					case Opcode::STORE_IND_FLOAT: {
						Variant& regA = current_frame->get_register(instr.a);
						Variant& regB = current_frame->get_register(instr.b);
						if (regA.is_ptr()) {
							*reinterpret_cast<f32*>(regA.get_ptr()) = regB.to_float();
						}
						else {
							lg::error("STORE_IND_FLOAT: Expected pointer, got {}", regA.to_string());
						}
						break;
					}

					case Opcode::STORE_IND_POINTER: {
						Variant& regA = current_frame->get_register(instr.a);
						Variant& regB = current_frame->get_register(instr.b);
						if (regA.is_ptr()) {
							*reinterpret_cast<void**>(regA.get_ptr()) = regB.get_ptr();
						}
						else {
							lg::error("STORE_IND_POINTER: Expected pointer, got {}", regA.to_string());
						}
						break;
					}

					// ============================================================
					// Static Load Instructions (0xB*)
					// ============================================================
					case Opcode::LOAD_STATIC_INT: {
						Variant& dest = current_frame->get_register(instr.a);
						u32 offset = instr.imm16;
						vm_int data_record = current_frame->get_static_int(offset);
						dest = Variant(data_record);
						break;
					}

					case Opcode::LOAD_STATIC_FLOAT: {
						Variant& dest = current_frame->get_register(instr.a);
						u32 offset = instr.imm16;
						vm_float data_record = current_frame->get_static_float(offset);
						dest = Variant(data_record);
						break;
					}

					case Opcode::LOAD_STATIC_POINTER: {
						Variant& dest = current_frame->get_register(instr.a);
						u32 offset = instr.imm16;
						const void* data_record = current_frame->get_static_pointer(offset);
						dest = Variant(data_record);
						break;
					}

					// ============================================================
					// Default case for unknown opcodes
					// ============================================================
					default: {
						lg::error("Unknown opcode: {} (0x{:02X})", static_cast<u32>(instr.opcode), static_cast<u32>(instr.opcode));
						current_frame = nullptr;
						break;
					}
				} // end of case
			} catch (const VmError& e) {
				VmError ne(e.get_message(), current_frame);
				lg::error("{}", ne.what());  // уже содержит информацию о фрейме
				current_frame = nullptr;
				is_error = true;
				break_reason = ne.what();
				break;
			} catch (const TypeError& e) {
				VmError ne(e.what(), current_frame);
				lg::error("{}", ne.what());  // уже содержит информацию о фрейме
				current_frame = nullptr;
				is_error = true;
				break_reason = ne.what();
				break;
			} catch (const std::exception& e) {
				// Оборачиваем в VmError
				lg::error("VM Exception: {} [PC={}]", e.what(), 
						current_frame ? current_frame->pc - 1 : 0);
				current_frame = nullptr;
				is_error = true;
				break_reason = e.what();
				break;
			} catch (...) {
				lg::error("VM Unknown exception [PC={}]", 
						current_frame ? current_frame->pc - 1 : 0);
				current_frame = nullptr;
				is_error = true;
				break_reason = "unknown exception";
				break;
			}
			if (mode == RunMode::Step) {
				is_break = true;
			}
		} // end of loop

		return final_result;
	}
} // namespace vm