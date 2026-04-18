#include "common/carbon/vm/VirtualMachine.hpp"
#include "common/carbon/vm/StackFrame.hpp"
#include "common/carbon/file/Export.hpp"
#include "common/util/Log.hpp"
#include "file/DCScript.hpp"
#include "lib/StringId.hpp"
#include "lib/Variant.hpp"

using namespace carbon;
using namespace carbon;


namespace carbon {

	// ------------------------------------------------------------------------
	// Internal Helpers
	// ------------------------------------------------------------------------

	i64 VirtualMachine::resolve_integer(std::shared_ptr<StackFrame> frame, StringId name) {
		/*
		auto module = frame->byte_code->owner_module;
		if (module) {
			Definition* resolved = module->resolve_symbol(name);
			if (resolved) {
				if (resolved->type == TypeIds::i64)
					return *((i64*)resolved->ptr.get());
				if (resolved->type == TypeIds::i32)
					return *((i32*)resolved->ptr.get());
				if (resolved->type == TypeIds::i16)
					return *((i16*)resolved->ptr.get());
				if (resolved->type == TypeIds::i8)
					return *((i8*)resolved->ptr.get());

				if (resolved->type == TypeIds::u64)
					return *((u64*)resolved->ptr.get());
				if (resolved->type == TypeIds::u32)
					return *((u32*)resolved->ptr.get());
				if (resolved->type == TypeIds::u16)
					return *((u16*)resolved->ptr.get());
				if (resolved->type == TypeIds::u8)
					return *((u8*)resolved->ptr.get());

            	throw VmTypeError("resolve_integer", frame, resolved->type);
			}
			else {
				throw VmResolvingError("resolve_integer", frame, name);
			}
		}
		*/
		throw VmResolvingError("resolve_integer", frame, name);
	}

	f64 VirtualMachine::resolve_float(std::shared_ptr<StackFrame> frame, StringId name) {
		/*
		auto module = frame->byte_code->owner_module;
		if (module) {
			Definition* resolved = module->resolve_symbol(name);
			if (resolved) {
				if (resolved->type == TypeIds::i64)
					return *((i64*)resolved->ptr.get());
				if (resolved->type == TypeIds::i32)
					return *((i32*)resolved->ptr.get());
				if (resolved->type == TypeIds::i16)
					return *((i16*)resolved->ptr.get());
				if (resolved->type == TypeIds::i8)
					return *((i8*)resolved->ptr.get());

				if (resolved->type == TypeIds::u64)
					return *((u64*)resolved->ptr.get());
				if (resolved->type == TypeIds::u32)
					return *((u32*)resolved->ptr.get());
				if (resolved->type == TypeIds::u16)
					return *((u16*)resolved->ptr.get());
				if (resolved->type == TypeIds::u8)
					return *((u8*)resolved->ptr.get());

            	throw VmTypeError("resolve_float", frame, resolved->type);
			}
			else {
				throw VmResolvingError("resolve_float", frame, name);
			}
		}
			*/
		throw VmResolvingError("resolve_float", frame, name);
	}

	void* VirtualMachine::resolve_pointer(std::shared_ptr<StackFrame> frame, StringId name) {
		/*
		auto module = frame->byte_code->owner_module;
		if (module) {
			Definition* resolved = module->resolve_symbol(name);
			if (resolved) {
				return resolved->ptr.get();
			}
			else {
				throw VmResolvingError("resolve_pointer", frame, name);
			}
		}
			*/
		throw VmResolvingError("resolve_pointer", frame, name);
	}

	// ------------------------------------------------------------------------
	// Process Management (НОВОЕ - согласно нашему базису)
	// ------------------------------------------------------------------------

	// Исполнение функции в модуле
	//Variant VirtualMachine::execute_function(Module* module, StringId function, RunMode mode)
	//{
	//	FunctionDesc* code = module->resolve_function(function);
//
	//	if (code) {
	//		return execute_function(code, mode);
	//	}
	//	else {
	//		lg::warn("VM: No main code found for process {}", module->name, function);
	//	}
//
	//	return Variant();
	//}

	// Исполнение кода 
	Variant VirtualMachine::execute_function(ScriptLambda* script_lambda, RunMode mode) {
		if (!script_lambda) {
			lg::error("Cannot execute null FunctionDesc");
			return Variant();
		}

		auto current_frame = create_stack_frame(
			script_lambda->get_code_ptr(),
			script_lambda->get_symbols_ptr(),
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
				lg::debug("PC={} : {}", current_frame->pc - 1, instr.to_string());

			try {

				// Исколнение конкретной инструкции
				switch (instr.opcode) {
					// ============================================================
					// Control Flow Instructions (0x0*)
					// ============================================================
					case Opcode::Return: {
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

					case Opcode::Move: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = src;
						break;
					}

					case Opcode::Call: {
						Variant& func_var = current_frame->get_register(instr.a);

						if (!func_var.is_ptr()) {
							lg::error("Call target is not a lambda: {}", func_var.to_string());
							current_frame = nullptr;
							break;
						}

						ScriptLambda* target_code = reinterpret_cast<ScriptLambda*>(func_var.get_ptr());
						auto new_frame = create_stack_frame(
							target_code->get_code_ptr(),
							target_code->get_symbols_ptr(),
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

					case Opcode::CallFf: {
						Variant& func_var = current_frame->get_register(instr.a);
						NativeFunction native_func = nullptr;

						if (func_var.is_ptr()) {
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

					case Opcode::Branch: {
						current_frame->pc = instr.imm16;
						break;
					}

					case Opcode::BranchIf: {
						Variant& condition = current_frame->get_register(instr.a);
						if (condition.to_bool()) {
							current_frame->pc = instr.imm16;
						}
						break;
					}

					case Opcode::BranchIfNot: {
						Variant& condition = current_frame->get_register(instr.a);
						if (!condition.to_bool()) {
							current_frame->pc = instr.imm16;
						}
						break;
					}

					// ============================================================
					// Integer Arithmetic Instructions (0x1*)
					// ============================================================
					case Opcode::IAdd: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() + src2.to_int());
						break;
					}

					case Opcode::ISub: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() - src2.to_int());
						break;
					}

					case Opcode::IMul: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() * src2.to_int());
						break;
					}

					case Opcode::IDiv: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						i32 divisor = src2.to_int();
						if (divisor == 0) {
							lg::error("Division by zero");
							break;
						}
						dest = Variant(src1.to_int() / divisor);
						break;
					}

					case Opcode::IMod: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						i32 divisor = src2.to_int();
						if (divisor == 0) {
							lg::error("Division by zero");
							break;
						}
						dest = Variant(src1.to_int() % divisor);
						break;
					}

					case Opcode::IAbs: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(std::abs(src.to_int()));
						break;
					}

					case Opcode::INeg: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(-src.to_int());
						break;
					}

					case Opcode::IntAsh: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						i32 value = src1.to_int();
						i32 shift = src2.to_int();
						dest = Variant(shift >= 0 ? value << shift : value >> -shift);
						break;
					}

					case Opcode::CastInteger: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(src.to_int());
						break;
					}

					// ============================================================
					// Integer Immediate Instructions (0x2*)
					// ============================================================
					case Opcode::LoadU16Imm: {
						Variant& dest = current_frame->get_register(instr.a_imm);
						dest = Variant(instr.imm16);
						break;
					}

					case Opcode::IAddImm: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						i32 imm = static_cast<i32>(instr.c);
						dest = Variant(src.to_int() + imm);
						break;
					}

					case Opcode::ISubImm: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						i32 imm = static_cast<i32>(instr.c);
						dest = Variant(src.to_int() - imm);
						break;
					}

					case Opcode::IMulImm: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						i32 imm = static_cast<i32>(instr.c);
						dest = Variant(src.to_int() * imm);
						break;
					}

					case Opcode::IDivImm: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						i32 imm = static_cast<i32>(instr.c);
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
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() + src2.to_float());
						break;
					}

					case Opcode::FSub: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() - src2.to_float());
						break;
					}

					case Opcode::FMul: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() * src2.to_float());
						break;
					}

					case Opcode::FDiv: {
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

					case Opcode::FMod: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(std::fmod(src1.to_float(), src2.to_float()));
						break;
					}

					case Opcode::FAbs: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(std::fabs(src.to_float()));
						break;
					}

					case Opcode::FNeg: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(-src.to_float());
						break;
					}

					case Opcode::CastFloat: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(src.to_float());
						break;
					}

					// ============================================================
					// Comparison Instructions (0x4*)
					// ============================================================
					case Opcode::IEqual: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() == src2.to_int());
						break;
					}

					case Opcode::IGreaterThan: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() > src2.to_int());
						break;
					}

					case Opcode::IGreaterThanEqual: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() >= src2.to_int());
						break;
					}

					case Opcode::ILessThan: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() < src2.to_int());
						break;
					}

					case Opcode::ILessThanEqual: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() <= src2.to_int());
						break;
					}

					case Opcode::FEqual: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(std::fabs(src1.to_float() - src2.to_float()) < 0.0001f);
						break;
					}

					case Opcode::FGreaterThan: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() > src2.to_float());
						break;
					}

					case Opcode::FGreaterThanEqual: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() >= src2.to_float());
						break;
					}

					case Opcode::FLessThan: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() < src2.to_float());
						break;
					}

					case Opcode::FLessThanEqual: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_float() <= src2.to_float());
						break;
					}

					// ============================================================
					// Logical Instructions (0x5*)
					// ============================================================
					case Opcode::OpLogAnd: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_bool() && src2.to_bool());
						break;
					}

					case Opcode::OpLogOr: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_bool() || src2.to_bool());
						break;
					}

					case Opcode::OpLogNot: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(!src.to_bool());
						break;
					}

					// ============================================================
					// Bitwise Instructions (0x6*)
					// ============================================================
					case Opcode::OpBitAnd: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() & src2.to_int());
						break;
					}

					case Opcode::OpBitOr: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() | src2.to_int());
						break;
					}

					case Opcode::OpBitXor: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(src1.to_int() ^ src2.to_int());
						break;
					}

					case Opcode::OpBitNor: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src1 = current_frame->get_register(instr.b);
						Variant& src2 = current_frame->get_register(instr.c);
						dest = Variant(~(src1.to_int() | src2.to_int()));
						break;
					}

					case Opcode::OpBitNot: {
						Variant& dest = current_frame->get_register(instr.a);
						Variant& src = current_frame->get_register(instr.b);
						dest = Variant(~src.to_int());
						break;
					}

					// ============================================================
					// Utility Instructions (0x7*)
					// ============================================================
					case Opcode::LoadParamCnt: {
						Variant& dest = current_frame->get_register(instr.a);
						dest = Variant(static_cast<i32>(current_frame->argc));
						break;
					}

					// ============================================================
					// Lookup Instructions (0x8*)
					// ============================================================
					case Opcode::LookupInt: {
						Variant& dest = current_frame->get_register(instr.a);
						auto name_id = StringId(current_frame->get_static_int(instr.imm16));
						dest = Variant(resolve_integer(current_frame, name_id));
						break;
					}

					case Opcode::LookupFloat: {
						Variant& dest = current_frame->get_register(instr.a);
						auto name_id = StringId(current_frame->get_static_int(instr.imm16));
						dest = Variant(resolve_float(current_frame, name_id));
						break;
					}

					case Opcode::LookupPointer: {
						Variant& dest = current_frame->get_register(instr.a);
						auto name_id = StringId(current_frame->get_static_int(instr.imm16));
						dest = Variant(resolve_pointer(current_frame, name_id));
						break;
					}


					// ============================================================
					// Indirect Load (через указатель)
					// ============================================================

					case Opcode::LoadInt:
					case Opcode::LoadI32: 
					case Opcode::LoadU32: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i32 value = *reinterpret_cast<i32*>(ptr);
						current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
						break;
					}
					case Opcode::LoadFloat: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						f32 value = *reinterpret_cast<f32*>(ptr);
						current_frame->get_register(instr.a) = Variant(static_cast<f64>(value));
						break;
					}

					case Opcode::LoadPointer: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						void* value = *reinterpret_cast<void**>(ptr);
						current_frame->get_register(instr.a) = Variant(value, RuntimeType::Pointer);
						break;
					}
					// ============================================================
					// Indirect Load (разные размеры) — 0x73..0x7B
					// ============================================================
					case Opcode::LoadI8: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i8 value = *reinterpret_cast<i8*>(ptr);
						current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
						break;
					}
					case Opcode::LoadU8: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						u8 value = *reinterpret_cast<u8*>(ptr);
						current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
						break;
					}
					case Opcode::LoadI16: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i16 value = *reinterpret_cast<i16*>(ptr);
						current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
						break;
					}
					case Opcode::LoadU16: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						u16 value = *reinterpret_cast<u16*>(ptr);
						current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
						break;
					}

					case Opcode::LoadI64: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i64 value = *reinterpret_cast<i64*>(ptr);
						current_frame->get_register(instr.a) = Variant(value);
						break;
					}
					case Opcode::LoadU64: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						u64 value = *reinterpret_cast<u64*>(ptr);
						current_frame->get_register(instr.a) = Variant(static_cast<i64>(value));
						break;
					}

					// ============================================================
					// Indirect Store Instructions (0xA*)
					// ============================================================
					case Opcode::StoreInt: 
					case Opcode::StoreI32:
					case Opcode::StoreU32: {
						Variant& regA = current_frame->get_register(instr.a);
						Variant& regB = current_frame->get_register(instr.b);
						if (regA.is_ptr()) {
							*reinterpret_cast<i32*>(regA.get_ptr()) = regB.to_int();
						}
						else {
							lg::error("STORE_IND_INT: Expected pointer, got {}", regA.to_string());
						}
						break;
					}

					case Opcode::StoreFloat: {
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

					case Opcode::StorePointer: {
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
					// Indirect Store (разные размеры) — 0x7C..0x8B
					// ============================================================
					case Opcode::StoreI8: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i64 value = current_frame->get_register(instr.a).get_i64();
						*reinterpret_cast<i8*>(ptr) = static_cast<i8>(value);
						break;
					}
					case Opcode::StoreU8: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i64 value = current_frame->get_register(instr.a).get_i64();
						*reinterpret_cast<u8*>(ptr) = static_cast<u8>(value);
						break;
					}
					case Opcode::StoreI16: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i64 value = current_frame->get_register(instr.a).get_i64();
						*reinterpret_cast<i16*>(ptr) = static_cast<i16>(value);
						break;
					}
					case Opcode::StoreU16: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i64 value = current_frame->get_register(instr.a).get_i64();
						*reinterpret_cast<u16*>(ptr) = static_cast<u16>(value);
						break;
					}
					case Opcode::StoreI64: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i64 value = current_frame->get_register(instr.a).get_i64();
						*reinterpret_cast<i64*>(ptr) = value;
						break;
					}
					case Opcode::StoreU64: {
						void* ptr = current_frame->get_register(instr.b).get_ptr();
						i64 value = current_frame->get_register(instr.a).get_i64();
						*reinterpret_cast<u64*>(ptr) = static_cast<u64>(value);
						break;
					}					
					// ============================================================
					// Indirect Load Instructions 
					// ------------------------------------------------------------
					// В этих командах номер константы в регистре а не в imediate operand
					// ============================================================

					case Opcode::LoadStaticInt: {
						// _RDI[_RBP] = symbol_table_ptr[_RDI[_RSI]]
						u64 index = current_frame->get_register(instr.b).get_i64();
						i64 value = current_frame->get_static_int(index);
						current_frame->get_register(instr.a) = Variant(value);
						break;
					}

					case Opcode::LoadStaticFloat: {
						u64 index = current_frame->get_register(instr.b).get_i64();
						f64 value = current_frame->get_static_float(index);  // ← get_static_float!
						current_frame->get_register(instr.a) = Variant(value);
						break;
					}

					case Opcode::LoadStaticPointer: {
						u64 index = current_frame->get_register(instr.b).get_i64();
						void* ptr = current_frame->get_static_pointer(index);  // ← get_static_pointer!
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
						u32 index = instr.b;  // ← байт 2 = operand1 = индекс в ST!
						
						if (instr.opcode == Opcode::LoadStaticFloatImm) {
							f64 value = current_frame->get_static_float(index);
							current_frame->get_register(instr.a) = Variant(value);
						} else if (instr.opcode == Opcode::LoadStaticPointerImm) {
							void* ptr = current_frame->get_static_pointer(index);
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
						
						void* src = current_frame->get_register(instr.c).get_ptr();  // _R15
						void* dst = current_frame->get_register(instr.b).get_ptr();  // _RSI
						
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