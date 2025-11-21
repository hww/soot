#include "printer.hpp"
#ifdef CUSTOM_PRINT
#include <cstdarg>
#endif
#include <format>

#include "environment.hpp"
#include "context.hpp"
#include "variant.hpp"
#include "module.hpp"
#include "vm.hpp"

namespace vm
{
	struct FGlobalDefinition;
#ifdef CUSTOM_PRINT
	void VariantPrintToFile(FILE* f, char* fmt, ...)
	{
		va_list lst;
		va_start(lst, fmt);
		while (*fmt != '\0')
		{
			if (*fmt != '%')
			{
				putchar(*fmt);
				fmt++;
				continue;
			}

			fmt++;

			if (*fmt == '\0')
			{
				break;
			}

			switch (*fmt)
			{
			case 's': fputs(va_arg(lst, char*), f); break;
			case 'c': fputc(va_arg(lst, int), f); break;
			case 'v': fputs(vm::ConvertToString(va_arg(lst, FVariant)).c_str(), f); break;
			}
			fmt++;
		}
	}

	void VariantPrint(char* fmt, ...)
	{
		va_list lst;
		va_start(lst, fmt);
		VariantPrintToFile(stdout, fmt, lst);
	}

	void VariantPrintError(char* fmt, ...)
	{
		va_list lst;
		va_start(lst, fmt);
		VariantPrintToFile(stderr, fmt, lst);
	}
#endif

	//--------------------------------------------------------------------------
	// Hex dump printer
	//--------------------------------------------------------------------------

	void PrintDump(u32 address, const u8* pSrc, size_t len)
	{
		for (u32 i = 0; i < len; i++)
		{
			if (i % 16 == 0)
			{
				if (i != 0)
					printf("\n");
				printf("0x%08x ", address + i);
			}
			else
			{
				if (i % 4 == 0)
					printf(" | ");
			}
			printf("%02x ", pSrc[i]);
		}
		printf("\n");
	}

	//--------------------------------------------------------------------------
	// The conversions to to the string
	//--------------------------------------------------------------------------

	DEFINE_TO_STR(FGlobalDefinition)
	DEFINE_TO_STR(FGlobalEnvironment)
	DEFINE_TO_STR(FLocalDefinition)
	DEFINE_TO_STR(FLocalEnvironment)
	DEFINE_TO_STR(FVariant)
	DEFINE_TO_STR(FStackFrame)
	DEFINE_TO_STR(FByteCode)
	DEFINE_TO_STR(FBinFile)
	DEFINE_TO_STR(FBinFileHeader)
	DEFINE_TO_STR(FScriptProcess)
	DEFINE_TO_STR(Module)


		/*!
	 * Print directly to the C stdout
	 * The "k" parameter is ignored, so this is just like printf
	 * DONE, EXACT
	 */
		void Msg(s32 k, const char* format, ...) {
		(void)k;
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}

	/*!
	 * Print directly to the C stdout
	 * This is idential to Msg
	 * DONE, EXACT
	 */
	void MsgWarn(const char* format, ...) {
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}

	/*!
	 * Print directly to the C stdout
	 * This is idential to Msg
	 * DONE, EXACT
	 */
	void MsgErr(const char* format, ...) {
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
}
