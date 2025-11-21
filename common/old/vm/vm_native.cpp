#include <iostream>
#include "native_func.hpp"
#include "string_id.hpp"
#include "variant.hpp"
#include "environment.hpp"

namespace vm
{
	//--------------------------------------------------------------------------
	// The native functions helpers
	//--------------------------------------------------------------------------
	FVariant ScriptPrint(const u32 argc, const FVariant* argv)
	{
		for (size_t i = 0; i < argc; i++)
			std::cout << argv[i].to_c_string() << " ";
		return FVariant{ true };
	}
	FVariant ScriptPrintLine(const u32 argc, const FVariant* argv)
	{
		ScriptPrint(argc, argv);
		std::cout << std::endl;
		return FVariant{ true };
	}

	//--------------------------------------------------------------------------
	// The native functions 
	//--------------------------------------------------------------------------

	FNativeFunctionEntry g_NativeFunctionTable[] = {
		{ SID("print"), ScriptPrint },
		{ SID("println"), ScriptPrintLine }
	};

	//--------------------------------------------------------------------------
	// The native functions 
	//--------------------------------------------------------------------------


#ifdef USE_GLOBAL_NATIVE_DEFINITIONS
	// Use global table for natives
#else 
	std::map<StringId, FNativeFunctionEntry*> g_NativeFunctionMap;
#endif

	
	void init_native_functions()
	{
#ifdef USE_GLOBAL_NATIVE_DEFINITIONS
		for (auto& t : g_NativeFunctionTable)
			g_environment.define(t.name, FGlobalDefinition(SID("native"), t));
#else 
		for (auto& t : g_NativeFunctionTable)
			g_NativeFunctionMap[t.Name] = &t;
#endif
	}

	FNativeFunction* lookup_native_function(StringId name) {
#ifdef USE_GLOBAL_NATIVE_DEFINITIONS
		auto ptr = g_environment.lookup(name);
		if (ptr == nullptr || ptr->type != SID("native"))
			return nullptr;
		return (FNativeFunction*)ptr->ptr;
#else 
		const auto func = g_NativeFunctionMap.find(name);
		return (func == g_NativeFunctionMap.end()) ? nullptr : func->second->Ptr;
#endif
	}
	

}
