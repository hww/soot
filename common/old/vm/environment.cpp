#include <format>
#include "environment.hpp"
#include "context.hpp"
#include "module.hpp"
#include "native_func.hpp"

namespace vm
{
	//--------------------------------------------------------------------------
	// The global environment 
	//--------------------------------------------------------------------------

	FGlobalEnvironment g_environment{};

	/**
	 * \brief Constructor for the global definition
	 * \param type - type of definition
	 * \param ptr - pointer of definition
	 */
	FGlobalDefinition::FGlobalDefinition(const StringId type, const FNativeFunctionEntry& ptr)
		: FLocalDefinition(type, (PTRINT)ptr.func)
	{}
	/**
	 * \brief Constructor for the global definition
	 * \param type - type of definition
	 * \param ptr - pointer of definition
	 * \param moduleRef - reference to the module
	 */
	FGlobalDefinition::FGlobalDefinition(const StringId type, const PTRINT ptr, const ModuleRef* moduleRef)
		: FLocalDefinition(type, ptr)
		, module_ref(moduleRef)
		, generation(moduleRef->generation)
	{}

	bool FGlobalDefinition::is_valid() const
	{
		if (type == SID("native"))
			return true;
		return module_ref == nullptr ? false : generation == module_ref->generation;
	}

	FGlobalDefinition& FGlobalDefinition::operator=(const FGlobalDefinition& other) = default;

	std::string FGlobalDefinition::to_string() const
	{
		return std::format("#FGlobalDefinition <this: {0} ptr: {1} type: {2} module: {3} generation: {4}>",
			to_str((PTRINT)this), to_str(ptr), to_str(type), to_str((PTRINT)module_ref), generation);
	}

	FGlobalDefinition* FGlobalEnvironment::lookup(const StringId name)
	{
		const auto definition = table.find(name);
		if (definition != table.end() && definition->second.is_valid())
			return &definition->second;
		return nullptr;
	}

	FGlobalDefinition* FGlobalEnvironment::lookup(const StringId name, const StringId type)
	{
		const auto definition = lookup(name);
		if (definition == nullptr)
			return nullptr;
		if (definition->type == type)
			return definition;
		throw FTypeError(g_script_context, name, type, definition->type);
	}
	//--------------------------------------------------------------------------
	// The local environment 
	//--------------------------------------------------------------------------


	FLocalDefinition* FLocalEnvironment::lookup(const StringId name, bool recursive)
	{
		const auto definition = table.find(name);
		if (definition != table.end())
			return &definition->second;
		if (recursive)
			return g_environment.lookup(name);
		return nullptr;
	}

	FLocalDefinition* FLocalEnvironment::lookup(const StringId name, const StringId type, bool recursive)
	{
		const auto definition = lookup(name, recursive);
		if (definition == nullptr)
			return nullptr;
		if (definition->type == type)
			return definition;
		throw FTypeError(g_script_context, name, type, definition->type);
	}

	FLocalDefinition& FLocalDefinition::operator=(const FLocalDefinition& other) = default;

	std::string FLocalDefinition::to_string() const
	{
		return std::format("#FLocalDefinition <ptr: {0} type: {1}>",
		                   to_str(ptr), to_str(type));
	}

}

