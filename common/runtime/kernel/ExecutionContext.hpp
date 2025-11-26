#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/lib/Types.hpp"
#include "common/runtime/files/BinaryFilePool.hpp"
#include "common/runtime/modules/ModuleRegistry.hpp"
#include "common/runtime/modules/ModuleManager.hpp"
#include "common/runtime/vm/VirtualMachine.hpp"
#include "common/runtime/kernel/ProcessScheduler.hpp" 
#include "common/runtime/kernel/NativeFunc.hpp"

using namespace runtime::files;
using namespace runtime::modules;

namespace runtime::kernel
{

	/*
	 *    // main.cpp
	 *    #include "execution_context.hpp"
	 *
	 *    int main() {
	 *        // Инициализация
	 *        if (!vm::ExecutionContext::instance().initialize()) {
	 *            return -1;
	 *        }
	 *
	 *        // Загрузка модуля и вызов функции
	 *        auto result = vm::ExecutionContext::instance().call_function(
	 *            "math", "add", { vm::Variant(10), vm::Variant(20) }
	 *        );
	 *
	 *        std::cout << "Result: " << result.to_string() << std::endl;
	 *
	 *        // Создание процесса
	 *        auto pid = vm::ExecutionContext::instance().create_process(
	 *            "game", "main_loop"
	 *        );
	 *        vm::ExecutionContext::instance().start_process(pid);
	 *
	 *        // Отладочная информация
	 *        vm::ExecutionContext::instance().dump_state();
	 *
	 *        // Деинициализация
	 *        vm::ExecutionContext::instance().deinitialize();
	 *        return 0;
	 *    }
	 */
	class ExecutionContext {
	private:
		ExecutionContext() = default;

	public:
		// Singleton
		static ExecutionContext& instance() {
			static ExecutionContext instance;
			return instance;
		}

		// Lifecycle
		bool initialize(u32 binary_file_pool_size = 1024 * 1024); // 1MB default
		void deinitialize();

		// Module management
		std::shared_ptr<Module> load_module(const std::string& name);
		bool unload_module(const std::string& name);

		// Execution
		Variant call_function(const std::string& module_name,
			const std::string& function_name,
			const std::vector<Variant>& args = {});

		// Process management  
		Process* create_process(const std::string& entry_module,
			const std::string& entry_function,
			const std::vector<Variant>& args = {});
		bool start_process(Process* pid);

		Variant execute(ByteCode* byte_code);

		// Debugging
		void dump_state() const;
		std::string to_string() const;

	private:
		void initialize_native_functions();

		// Components
		ModuleRegistry* module_registry_ = nullptr;
		BinaryFilePool* binary_files_ = nullptr;  // исправлена опечатка
		ModuleManager* module_manager_ = nullptr;
		Scheduler* scheduler_ = nullptr;
		VirtualMachine* vm_ = nullptr;

		bool initialized_ = false;
	};

} // namespace vm