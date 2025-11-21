#pragma once
#include "config.hpp"
#include "errors.hpp"
#include "context.hpp"
#include "variant.hpp"

namespace vm
{

	/**
	 * @brief Can be used inside a FNativeFunction function only
	 *        The example of usage below
	 *        StringId objName = SC_ARG(0,StringId, NULL);
	 *        StringId animName = SC_ARG(1,StringId, NULL);
	 * @param argNum - Argument number
	 * @param argType - Argument type
	 * @param defVal - Return value if there is no argument with this number
	 */
#define SC_ARG(arg_num, arg_type, def_val) (arg_num >= argc ? def_val :  argv[arg_num].##arg_type)

	/**
	 * @brief The native function type
	 * @param argc - Arguments count
	 * @param argv - Arguments list
	 * @return The result of function as variant class
	 */
	using FNativeFunction = FVariant(const u32 argc, const FVariant* argv);

	/**
	 * @brief The structure contains list of native functions
	 *		  visible for the script.
	 */
	struct FNativeFunctionEntry
	{
		/** @brief The name of the native function */
		StringId name;
		/** @brief The pointer to native function */
		FNativeFunction* func;
	};

	/**
	 * @brief Find the native function by name
	 * @param name - the name of native function
	 * @return A pointer to native function or nullptr
	 */
	FNativeFunction* lookup_native_function(StringId name);
	/**
	 * @brief Initialize the list of native functions
	 */
	void init_native_functions();
}
