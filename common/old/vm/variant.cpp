#include "variant.hpp"

#include <cstdarg>

#include "errors.hpp"
#include "context.hpp"

namespace vm
{
	// --------------------------------------------------------------------
	// Constructor
	// --------------------------------------------------------------------

	FVariant::~FVariant()
	{
		before_set();
	}

	// --------------------------------------------------------------------
	// Garbage
	// --------------------------------------------------------------------

	// Check if the memory releasing 
	inline void FVariant::before_set()
	{
		if (type != EType::I32 && type != EType::F32)
		{
			// TODO There should be code with for destroying
			// the referenced objects in the pull
		}
		as_ptr = 0;
		type = EType::NIL;
	}

	// --------------------------------------------------------------------
	// Setters
	// --------------------------------------------------------------------

	void FVariant::set_as_null() { before_set(); type = EType::NIL; }
	void FVariant::set_as_s32(const s32 value) { before_set(); as_s32 = value; type = EType::I32; }
	void FVariant::set_as_f32(const float value) { before_set(); as_f32 = value; type = EType::F32; }
	void FVariant::set_as_ptr(const PTRINT value) { before_set(); as_ptr = value; type = EType::PTR; }
	void FVariant::set_as_ptr(const PTRINT value, StringId _type) { before_set(); as_ptr = value; type = _type; }
	void FVariant::set(const FVariant& other) { before_set(); as_ptr = other.as_ptr; }

	// --------------------------------------------------------------------
	// Getters with casting
	// --------------------------------------------------------------------

	s32 FVariant::cast_to_s32() const
	{
		if (type == EType::I32)
			return as_s32;
		if (type == EType::F32)
			return static_cast<int>(as_f32);
		throw FRuntimeError(g_script_context, "cast-to-int : the value is '%s'.\n", get_type_string());
	}

	float FVariant::cast_to_f32() const
	{
		if (type == EType::I32)
			return static_cast<float>(as_s32);
		if (type == EType::F32)
			return as_f32;
		throw FRuntimeError(g_script_context, "cast-to-float : the value is '%s'.\n", get_type_string());
	}

	// --------------------------------------------------------------------
	// Getters
	// --------------------------------------------------------------------

	s32 FVariant::get_as_s32() const
	{
		if (type == EType::I32)
			return as_s32;
		throw FRuntimeError(g_script_context, "get-integer: the value is '%s'.\n", get_type_string());
	}

	float FVariant::get_as_f32() const
	{
		if (type == EType::F32)
			return as_f32;
		throw FRuntimeError(g_script_context, "get-float: the value is '%s'.\n", get_type_string());
	}

	PTRINT FVariant::get_as_ptr() const
	{
		if (type == EType::NIL || type == EType::I32 || type == EType::F32)
			throw FRuntimeError(g_script_context, "get-pointer: the value is '%s'.\n", get_type_string());
		return as_ptr;
	}


	bool FVariant::get_as_bool() const
	{
		switch (type) {
		case EType::NIL: return false;
		case EType::I32: return as_s32 != 0;
		case EType::F32: return as_f32 != 0;
		default:
			return as_ptr != 0;
		}
	}

	// --------------------------------------------------------------------
	// Debugging purposes
	// --------------------------------------------------------------------

	const char* FVariant::to_c_string() const
	{
		static char str[256];
		switch (type)
		{
		case EType::NIL:
			return "nil";
		case EType::I32:
			sprintf_s(str, "%i", as_s32);
			break;
		case EType::F32:
			sprintf_s(str, "%f", as_f32);
			break;
		default:
			sprintf_s(str, "#ptr<%p type %s>", (void*)as_ptr, to_str(type).c_str());
		}
		return str;
	}

	std::string FVariant::to_string() const
	{
		return to_c_string();
	}

	// --------------------------------------------------------------------
	// Math
	// --------------------------------------------------------------------


	bool FVariant::operator ==(const FVariant& other) const
	{
		if (type != other.type)
			return false;
		return as_ptr == other.as_ptr;
	}

	bool FVariant::operator !=(const FVariant& other) const
	{
		return !(*this == other);
	}

	FVariant& FVariant::operator =(const FVariant& other)
	{
		type = other.type;
		as_ptr = other.as_ptr;
		return *this;
	}

	// =============================================================================
	// Global method to produce the error
	// =============================================================================

	// return variant error and printout the message
	// if there is active context then this method will 
	// set the process to sleep
	FVariant FVariant::print_error(const char* format, ...)
	{
		// [1] print the message to string
		char str[256];
		va_list arglist;
		va_start(arglist, format);
		vsprintf_s(str, format, arglist);
		va_end(arglist);
		// [2] print the message to log
		LOG_WARNING(str);
		// [3] make variant error
		FVariant res(false);
		return res;
	};


}
