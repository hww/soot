#include "errors.hpp"
#include "vm.hpp"
#include "context.hpp"

namespace vm
{
	FRuntimeError::FRuntimeError(const Context& context, const std::string& msg) : FException()
	{
		message = std::format("{0} : {1}", context.to_string(), msg);
	}

	FRuntimeError::FRuntimeError(const Context& context, const std::string& msg1, const std::string& msg2) : FException()
	{
		message = std::format("{0} : {1} : {2}", context.to_string(), msg1, msg2);
	}

	FRuntimeError::FRuntimeError(const Context& context, const std::string& msg1, const std::string& msg2,
		const std::string& msg3) : FException()
	{
		message = std::format("{0} : {1} : {2} : {3}", context.to_string(), msg1, msg2, msg3);
	}


	FTypeError::FTypeError(const Context& context, const StringId def_name, const StringId type_name,
		const StringId found_type_name)
	{
		message = std::format("{0} : Definition {1} expected type {2} found {3}",
			context.to_string(),
			lookup_string_safe(def_name),
			lookup_string_safe(type_name),
			lookup_string_safe(found_type_name));
	}

	FTypeError::FTypeError(const Context& context, const StringId def_name, const StringId type_name,
		const StringId found_type_name, const std::string& msg)
	{
		message = std::format("{0} : Definition {1} expected type {2} found {3} : {4}", 
			context.to_string(), 
			lookup_string_safe(def_name), 
			lookup_string_safe(type_name), 
			lookup_string_safe(found_type_name), 
			msg);
	}
}
