#include "common/runtime/files/BinaryFilePool.hpp"
#include "common/runtime/modules/ModuleManager.hpp"
#include "common/runtime/modules/ModuleRegistry.hpp"
#include "common/runtime/kernel/ExecutionContext.hpp"
#include "common/runtime/kernel/NativeFunc.hpp"
#include "common/runtime/vm/VirtualMachine.hpp"
#include "common/runtime/lib/Types.hpp"
#include "common/util/Log.hpp"

namespace vm
{
	bool ExecutionContext::initialize(u32 binary_file_pool_size) {
		if (initialized_) {
			lg::warn("ExecutionContext already initialized");
			return true;
		}

		try {
			lg::info("Initializing Execution Context...");

			// 1. Binary file pool
			if (!BinaryFilePool::initialize(binary_file_pool_size)) {
				lg::error("Failed to initialize BinaryFilePool");
				return false;
			}
			binary_files_ = nullptr; // BinaryFilePool is static

			// 2. Module registry (singleton)
			module_registry_ = &ModuleRegistry::instance();
			module_registry_->add_search_path("./modules");
			module_registry_->scan_and_index();

			// 3. Module manager (singleton)  
			module_manager_ = &ModuleManager::instance();

			// 4. Virtual Machine
			vm_ = new VirtualMachine();

			// 5. Scheduler
			scheduler_ = new Scheduler();
			scheduler_->initialize();

			// 6. Native functions
			initialize_native_functions();

			initialized_ = true;
			lg::info("ExecutionContext initialized successfully (pool: {} bytes)",
				binary_file_pool_size);
			return true;

		}
		catch (const std::exception& e) {
			lg::error("ExecutionContext initialization failed: {}", e.what());
			deinitialize();
			return false;
		}
	}

	void ExecutionContext::deinitialize() {
		if (!initialized_) return;

		lg::info("Deinitializing Execution Context...");

		delete scheduler_;
		scheduler_ = nullptr;

		delete vm_;
		vm_ = nullptr;

		BinaryFilePool::shutdown();

		initialized_ = false;
		lg::info("ExecutionContext deinitialized");
	}

	void ExecutionContext::initialize_native_functions() {
		auto& registry = NativeFunctionRegistry::get_instance();
		registry.initialize_builtins();

		// Add our custom natives
		REGISTER_NATIVE_FUNCTION("print", [](u32 argc, const Variant* argv) -> Variant {
			for (u32 i = 0; i < argc; i++) {
				lg::print("{} ", argv[i].to_string());
			}
			return Variant(true);
			});

		REGISTER_NATIVE_FUNCTION("println", [](u32 argc, const Variant* argv) -> Variant {
			for (u32 i = 0; i < argc; i++) {
				lg::print("{} ", argv[i].to_string());
			}
			lg::print("\n");
			return Variant(true);
			});

		lg::debug("Initialized {} native functions",
			NativeFunctionRegistry::get_instance().function_count());
	}

	// === MODULE MANAGEMENT ===
	std::shared_ptr<Module> ExecutionContext::load_module(const std::string& name) {
		if (!initialized_) {
			lg::error("ExecutionContext not initialized");
			return nullptr;
		}

		auto sid = string_id::register_string(name);
		return module_manager_->load_module(sid);
	}

	bool ExecutionContext::unload_module(const std::string& name) {
		if (!initialized_) return false;

		auto sid = string_id::register_string(name);
		module_manager_->unload_module(sid);
		return true;
	}

	// === EXECUTION ===
	Variant ExecutionContext::call_function(const std::string& module_name,
		const std::string& function_name,
		const std::vector<Variant>& args) {
		if (!initialized_) {
			lg::error("ExecutionContext not initialized");
			return Variant();
		}

		try {
			// 1. Load module if needed
			auto module = load_module(module_name);
			if (!module) {
				lg::error("Module not found: {}", module_name);
				return Variant();
			}

			// 2. Find function
			auto func_sid = string_id::register_string(function_name);
			auto bytecode = module->resolve_code(func_sid);
			if (!bytecode) {
				lg::error("Function not found: {}::{}", module_name, function_name);
				return Variant();
			}

			// 3. Prepare arguments (if we had argument passing)
			// TODO: Implement proper argument passing to VM

			// 4. Execute
			lg::debug("Calling {}.{}", module_name, function_name);
			return vm_->execute_bytecode(bytecode);

		}
		catch (const std::exception& e) {
			lg::error("Function call failed: {}.{} - {}",
				module_name, function_name, e.what());
			return Variant();
		}
	}

	// === PROCESS MANAGEMENT ===
	Process* ExecutionContext::create_process(const std::string& entry_module,
		const std::string& entry_function,
		const std::vector<Variant>& args) {
		if (!initialized_) return INVALID_PROCESS_ID;

		// TODO: Implement process creation
		lg::debug("Creating process: {}.{}", entry_module, entry_function);
		return scheduler_->create_process(entry_module, entry_function, args);
	}

	bool ExecutionContext::start_process(Process* pid) {
		if (!initialized_) return false;
		return scheduler_->start_process(pid);
	}

	// === DEBUGGING ===
	void ExecutionContext::dump_state() const {
		if (!initialized_) {
			lg::info("ExecutionContext not initialized");
			return;
		}

		lg::info("=== Execution Context State ===");
		lg::info("Initialized: {}", initialized_);

		if (vm_) vm_->dump_state();
		if (scheduler_) scheduler_->dump_process_list();

		lg::info("BinaryFilePool: {}", BinaryFilePool::stats());
	}

	std::string ExecutionContext::to_string() const {
		if (!initialized_) return "ExecutionContext[not initialized]";

		return fmt::format("ExecutionContext[vm:{}, scheduler:{}]",
			vm_ ? vm_->to_string() : "null",
			scheduler_ ? scheduler_->to_string() : "null");
	}

	Variant ExecutionContext::execute(ByteConde* byte_code) {
		vm_->execute(byte_code);
	}

} // namespace vm