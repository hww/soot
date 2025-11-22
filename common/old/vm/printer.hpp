#pragma once

#include <cstdarg>

#include "platform.hpp"
#include "string_id.hpp"

namespace vm
{
	struct FVariant;
	struct FLocalDefinition;
	struct FGlobalDefinition;
	struct FLocalEnvironment;
	struct FGlobalEnvironment;
	struct FStackFrame;
	struct ByteCode;
	struct BinaryFile;
	struct BinFileHeader;
	struct FScriptProcess;
	class Module;

#ifdef CUSTOM_PRINT
	void VariantPrintToFile(FILE* f, char* fmt, ...);
#endif

	void PrintDump(u32 address, const u8* pSrc, size_t len);

	// --------------------------------------------------------------------
	// Default printers
	// --------------------------------------------------------------------

	inline std::string to_str(s32 v) { return std::format("{0}",v); }
	inline std::string to_str(float v) { return std::format("{0}", v); }
	inline std::string to_str(PTRINT p) { return std::format("{0:08X}", p); }



	#define DECLARE_TO_STR(T) std::string to_str(const T* obj);
	#define DEFINE_TO_STR(T) \
	std::string to_str(const T* obj) { \
		if (obj == nullptr) return std::format("#{0} <null>", typeid(T).name()); \
		return obj->to_string(); }

	DECLARE_TO_STR(FGlobalDefinition)
	DECLARE_TO_STR(FGlobalEnvironment)
	DECLARE_TO_STR(FLocalDefinition)
	DECLARE_TO_STR(FLocalEnvironment)
	DECLARE_TO_STR(FVariant)
	DECLARE_TO_STR(FStackFrame)
	DECLARE_TO_STR(ByteCode)
	DECLARE_TO_STR(BinaryFile)
	DECLARE_TO_STR(BinFileHeader)
	DECLARE_TO_STR(FScriptProcess)
	DECLARE_TO_STR(Module)

	/*!
	 * Print directly to the C stdout
	 * The "k" parameter is ignored, so this is just like printf
	 * DONE, EXACT
	 */
	void Msg(s32 k, const char* format, ...);

	/*!
	 * Print directly to the C stdout
	 * This is idential to Msg
	 * DONE, EXACT
	 */
	void MsgWarn(const char* format, ...);

	/*!
	 * Print directly to the C stdout
	 * This is idential to Msg
	 * DONE, EXACT
	 */
	void MsgErr(const char* format, ...);

}
