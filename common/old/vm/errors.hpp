#pragma once

#include <string>
#include <exception>
#include <format>

#include "platform.hpp"
#include "string_id.hpp"

namespace vm
{
	class FException : public std::exception {
	public:
		FException()
			: std::exception()
		{
		}
		FException(const std::string& msg)
			: std::exception()
		{
			message = std::format("{0} : {1}", msg, std::exception::what());
		}


		virtual const char* what() const {
			return message.c_str();
		}
	protected:
		std::string message;
	};

	class FStringCollisionException final : public FException {

	public:
		FStringCollisionException(std::string str1, std::string str2)
			: FException()
		{
			message = std::format("StringId found collision of strings '{0}' and '{1}' : {2}\n",
				str1, str2, FException::what());
		}
		const char* what() const override
		{
			return message.c_str();
		}
	};

	struct Location
	{
		u32 line;
		u32 pos;

		Location() : line(0), pos(0) { }

		std::string to_str() const {
			return std::format("{0} : {1}", line, pos);
		}
	};

	class FSyntaxError : public FException {
	public:
		FSyntaxError() : FException()
		{ }

		FSyntaxError(const std::string& msg)
			: FException(msg)
		{
		}
		FSyntaxError(const Location& loc, const std::string& msg)
			: FException()
		{
			message = std::format("{0} : {1} : {2}", loc.line, loc.pos, msg);
		}
		FSyntaxError(const Location& loc, const std::string& msg1, const std::string& msg2)
			: FException()
		{
			message = std::format("{0} : {1} : {2} : {3}", loc.line, loc.pos, msg1, msg2);
		}
		FSyntaxError(const Location& loc, const std::string& msg1, const std::string& msg2, const std::string& msg3)
			: FException()
		{
			message = std::format("{0} : {1} : {2} : {3} : {4}", loc.line, loc.pos, msg1, msg2, msg3);
		}
	};

	struct Context;

	class FRuntimeError : public FException {
	public:
		FRuntimeError() : FException()
		{ }

		FRuntimeError(const Context& context, const std::string& msg);

		FRuntimeError(const Context& context, const std::string& msg1, const std::string& msg2);

		FRuntimeError(const Context& context, const std::string& msg1, const std::string& msg2, const std::string& msg3);

	};

	class FTypeError : public FException {
	public:
		FTypeError() : FException()
		{ }
		FTypeError(const Context& context, const StringId def_name, const StringId type_name, const StringId found_type_name);

		FTypeError(const Context& context, const StringId def_name, const StringId type_name, const StringId found_type_name, const std::string& msg);

	};




}
