#include "context.hpp"

namespace vm
{
	Context g_script_context{};

	PTRINT Context::lookup(StringId name) const
	{
		assert(process != nullptr && process->env != nullptr);
		const auto def = process->env->lookup(name);
		if (def != nullptr)
			return def->ptr;
		const auto global = g_environment.lookup(name);
		if (def != nullptr)
			return global->is_valid() ? global->ptr : 0;
		return 0;
	}
	PTRINT Context::lookup(StringId name, StringId type) const
	{
		assert(process != nullptr && process->env!=nullptr);
		const auto def = process->env->lookup(name);
		if (def != nullptr)
		{
			if (def->type != type)
				throw FTypeError(g_script_context, name, type, def->type);
			return def->ptr;
		}
		const auto global = g_environment.lookup(name);
		if (def != nullptr && global->is_valid())
		{
			if (def->type != type)
				throw FTypeError(g_script_context, name, type, def->type);
			return def->ptr;
		}
		return 0;
	}

	FByteCode* Context::lookup_byte_code(StringId name) const
	{
		const auto ptr = lookup(name,SID("lambda"));
		return ptr != 0 ? (FByteCode*)ptr : nullptr;
	}

	s32* Context::lookup_int(StringId name) const
	{
		const auto ptr = lookup(name, SID("s32"));
		return ptr != 0 ? (s32*)ptr : nullptr;
	}

	float* Context::lookup_float(StringId name) const
	{
		const auto ptr = lookup(name, SID("float"));
		return ptr != 0 ? (float*)ptr : nullptr;
	}

	StringId* Context::lookup_string_id(StringId name) const
	{
		const auto ptr = lookup(name, SID("string-id"));
		return ptr != 0 ? (StringId*)ptr : nullptr;
	}

}