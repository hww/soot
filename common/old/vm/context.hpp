#pragma once

#include <string>
#include <format>
#include "environment.hpp"
#include "vm.hpp"
#include "printer.hpp"

namespace vm
{
	enum class EScriptState
	{
		Suspend = 1 << 0,
		Complete = 1 << 2,
		Error = 1 << 3,
	};
	ENUM_FLAG_OPERATORS(EScriptState);

	struct FScriptProcess
	{
		PTRINT self;
		EScriptState state;
		FStackFrame* frame;
		FLocalEnvironment* env;

		void exit() { state &= ~EScriptState::Complete; }
		void suspend() { state |= EScriptState::Suspend; }
		void resume() { state &= ~EScriptState::Suspend; }
		void error() { state &= ~EScriptState::Error; }

		/**
		 * @brief Convert to string this thread
		 * @return string value for this process
		 */
		std::string to_string() const
		{
			return std::format("#FScriptProcess <self: {0} frame: {1} env: {2}>",
				to_str(self), to_str(frame), to_str(env));
		}
	};



	/**
	 * @brief Global object for manipulating by the current script thread
	 */
	struct Context
	{
		Context() : process(nullptr) {}


		void exit() const { if (process!=nullptr) process->exit(); }
		void suspend() { if (process != nullptr) process->suspend(); }
		void resume() { if (process != nullptr) process->resume(); }
		void error() { if (process != nullptr) process->error(); }

		PTRINT lookup(StringId name) const;
		/**
		 * @brief Look up bytecode by name
		 * @param name - the name of bytecode
		 * @return The pointer to object or nullptr
		 */
		PTRINT lookup(StringId name, StringId type) const;
		/**
		 * @brief Look up bytecode by name
		 * @param name - the name of bytecode
		 * @return The pointer to the bytecode pointer or nullptr
		 */
		FByteCode* lookup_byte_code(StringId name) const;
		/**
		 * \brief Find the integer value
		 * \param name - the name
		 * \return The pointer to integer or null
		 */
		s32* lookup_int(StringId name) const;
		/**
		 * \brief Find the float value
		 * \param name - the name
		 * \return The pointer to floating point value or null
		 */
		float* lookup_float(StringId name) const;
		/**
		 * \brief Find the string id
		 * \param name - the name
		 * \return The pointer to string id or null
		 */
		StringId* lookup_string_id(StringId name) const;
		/**/
		std::string to_string() const { return std::format("#s-process <{0:X}>", (PTRINT)process); }
		/**
		 * @brief The pointer to current process
		 */
		FScriptProcess* process;
	};
	/**
	 * @brief The global variable for access to the script environment
	*/
	extern Context g_script_context;

}
