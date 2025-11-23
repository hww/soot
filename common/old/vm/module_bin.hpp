#pragma once

#include <string>
#include <cassert>
#include <vector>
#include <format>
#include "errors.hpp"
#include "printer.hpp"
#include "variant.hpp"

namespace vm
{
	struct FBinFile;
	struct FBinFileHeader;

	/**
	 * Align the size of object to the biggies rounded value
	 */
	#define ALIGN_SIZE(n) ((n+3)&~3)

#undef REGISTER_OPCODE
#include "vm_opcodes.hpp"

	/** The virtual machine instruction. */
	__declspec(align(4))
		struct FInstr
	{
		union
		{
			u32 as_uint32;
			struct
			{
				/** The operation code */
				EOpcode opcode;
				/** Operand a */
				u8 a;
				/** Operand b */
				u8 b;
				/** Operand c */
				u8 c;
			};
			struct
			{
				/** The operation code */
				u8 skip[2];
				s16 k;
			};
		};

		FInstr(EOpcode op, s8 a, s8 b, s8 c) : opcode(op), a(a), b(b), c(c) {}
		FInstr(EOpcode op, s8 a, u16 k) : opcode(op), a(a), b(k & 0xFF), c(k >> 8) {}
		FInstr(EOpcode op, s8 a, s16 k) : opcode(op), a(a), b(k & 0xFF), c(k >> 8) {}


	};

	/** Single record in the data block */
	struct  FRecord
	{
		union
		{
			u64 as_u64;
			s32 as_s32;
			u32 as_u32;
			u16 as_u32;
			u8  as_u32;
			f32 as_f32;
			char as_char;
		};
	};

	/** The virtual machine byte code. */
	__declspec(align(8))
	struct  FByteCode  {

		void Initialize(FBinFileHeader* filePtr, const std::vector<FInstr>& code, const std::vector<FRecord>& data) {
			// Compute data needed for the all data
			constexpr auto headSize = sizeof(FByteCode);
			const auto codeSize = safe_cast_u_int32(sizeof(FInstr) * code.size());
			const auto dataSize = safe_cast_u_int32(sizeof(FRecord) * data.size());
			const auto totalSize = safe_cast_u_int32(headSize + codeSize + dataSize);
			// Get the offset form begin of file, the byte code address minus the file start address
			file_offs = safe_cast_to_int32(reinterpret_cast<PTRINT>(this) - reinterpret_cast<PTRINT>(filePtr));
			desc_size = totalSize;
			code_offs = file_offs + headSize;
			data_offs = code_offs + codeSize;
			// Copy code
			auto code_ptr = get_code_ptr();
			auto data_ptr = get_data_ptr();
			for (size_t i = 0; i < code.size(); i++)
				code_ptr[i] = code[i];
			for (size_t i = 0; i < data.size(); i++)
				data_ptr[i] = data[i];
		}



		FInstr* get_code_ptr()
		{
			return (FInstr*)((PTRINT)this + (code_offs - file_offs));
		}
		FRecord* get_data_ptr()
		{
			return (FRecord*)((PTRINT)this + (data_offs - file_offs));
		}
		u32 get_code_size() const
		{
			return (data_offs - code_offs) / sizeof(FInstr);
		}
		u32 get_data_size() const
		{
			return (desc_size - (data_offs - file_offs)) / sizeof(FInstr);
		}

		/**
		 * @brief Convert to string this file
		 * @return string value for this file
		 */
		std::string to_string() const
		{
			return std::format("#FByteCode <size: {0} offset: {1} code: {2} data: {3}>",
				desc_size, file_offs, code_offs, data_offs);
		}

		/* DescSize of the data and code */
		u32 desc_size;
		u32 file_offs;
		u32 code_offs;
		u32 data_offs;
	};

	/** The virtual machine definition. */
	__declspec(align(4))
		struct FDefinition {
		/** The definition's name */
		StringId Name;
		/** The definition's type */
		StringId Type;
		/** The definition's offset from begin of file */
		u32 Offset;

		/**
		 * @brief Initialize the definition, aka set 'name, 'type and offset from begin gile
		 * @param name - definition name
		 * @param type - definition type as StringId
		 * @param offset - definition offset from begin of file
		 */
		void Define(const StringId name, const StringId type, u32 offset);

	};

	/** The magic number in the file header */
	constexpr u32 DC_MAGIC('D' | 'X' << 8 | '0' << 16 | '0' << 24);

	/** The header of the binary file */
	__declspec(align(8))
		struct FBinFileHeader
	{

		FBinFileHeader(StringId id)
			: magic_num(DC_MAGIC)
			, file_size(0), used_size(sizeof(FBinFileHeader)), offset(0), defs_max(0), defs_num(0), defs_offs(0)
		{
		}

		/** Verify if the header is valid */
		bool is_valid_magic() const { return magic_num == DC_MAGIC; }

		/** Get a free size of the file */
		size_t get_free_size() const { return file_size - used_size; }
		/**
		 * Initialize the file header
		 * @params maxDefinitions - The maximum number of definitions in the file
		 */
		void init_definitions_table(u32 maxDefinitions, u32 fileSize)
		{
			magic_num = DC_MAGIC;
			defs_num = 0;
			offset = 0;
			defs_max = maxDefinitions;
			file_size = fileSize;
			defs_offs = sizeof(FBinFileHeader);
			used_size = defs_offs + sizeof(FDefinition) * maxDefinitions;
		}

		/**
		 * @brief Get reference to the definition
		 * @param idx - Index of the definition
		 * @return - Reference to the definition
		 */
		FDefinition* get_definition(size_t idx) const {
			if (idx >= defs_max)
				throw std::exception("The index of definition is overflow.");
			//const auto ptr1 = (FDefinition*)((const char*)this + DefsOffs);
			const auto ptr = reinterpret_cast<FDefinition*>(reinterpret_cast<PTRINT>(this) + defs_offs);
			return &ptr[idx];
		}


		template<class T>
		T* Define(const StringId name, const StringId type) {
			return static_cast<T*>(define(name, type));
		}

		PTRINT define(const StringId name, const StringId type) {

			if (defs_num >= defs_max)
				throw std::exception("The number of definition is overflow.");

			// Get free definition
			const auto definition = get_definition(defs_num);
			definition->Define(name, type, safe_cast_u_int32(used_size));
			defs_num++;
			return reinterpret_cast<PTRINT>(this) + used_size;
		}
		/**
		 * @brief Get definitions pointer aka pointer to int32, bool, lambda, ...
		 * @tparam T - type of returned pointer
		 * @param idx - index of definition
		 * @return pointer to int32, bool, lambda, ...
		 */
		template <typename T>
		T* get_definition_ptr(size_t idx) const
		{
			return (T*)get_definition_ptr(idx);
		}
		/**
		 * @brief Get definitions pointer aka pointer to int32, bool, lambda, ...
		 * @param idx - index of definition
		 * @return pointer to int32, bool, lambda, ...
		 */
		PTRINT get_definition_ptr(size_t idx) const
		{
			const auto defOffset = get_definition(idx)->Offset;
			return reinterpret_cast<PTRINT>(this) + defOffset;
		}

		void initialize(FByteCode* byteCode, const std::vector<FInstr>& code, const std::vector<FRecord>& data)
		{
			byteCode->Initialize(this, code, data);
			used_size += byteCode->desc_size;
		}

		/**
		 * @brief Convert to string this file
		 * @return string value for this file
		 */
		std::string to_string() const
		{
			return std::format("#FBinFileHeader <file-size: {0} used-size: {1} definitions-num: {2}>",
				file_size, used_size, defs_num);
		}

		/** Just a constant to validate file */
		u32 magic_num;
		/** The data block size in memory */
		u32 file_size;
		/** DescSize of used data */
		u32 used_size;
		/** Maximum allowed definitions */
		u32 defs_max;
		/** Used definitions */
		u32 defs_num;
		/** The definitions offset */
		u32 defs_offs;
		/** All pointer are incremented by this value */
		PTRINT offset;

	};


	struct FBinFile final
	{

	public:
		FBinFile()
			: file(nullptr), is_loaded(false)
		{
			
		}

		void initialize(u32 maxDefinitions, size_t dataSize)
		{
			assert(dataSize < std::numeric_limits<u32>::max());
			free(file);
			is_loaded = false;
			const size_t fileSize = sizeof(FBinFileHeader) + sizeof(FDefinition) * maxDefinitions + dataSize;
			file = reinterpret_cast<FBinFileHeader*>(new u8[fileSize]);
			file->init_definitions_table(maxDefinitions, static_cast<u32>(fileSize));
			printf("%p FBinFile with size %zx\n", static_cast<void*>(file), fileSize);
		}
		
		~FBinFile() {
			free(file);
		}

		void load_file(std::ifstream& stream);
		void save_file(std::ofstream& out) const;

		/**
		 * @brief Define new object in the file
		 * @tparam T - the class of the object
		 * @param name - the name of the object
		 * @param type - type of the object
		 * @return - reference to the T
		 */
		template<class T>
		T* define(const StringId name, const StringId type)
		{
			return (T*)define(name, type);
		}

		PTRINT define(const StringId name, const StringId type)
		{
			if (file == nullptr)
				throw FException("Dot use destructed object");
			return file->define(name, type);
		}

		size_t get_used_size() const { return file == nullptr ? 0 : file->used_size; }
		size_t get_free_size() const { return file == nullptr ? 0 : file->get_free_size(); }

		u32 get_def_number() const { return file->defs_num; }

		FDefinition* get_definition(u32 idx) const { return file->get_definition(idx); }

		FBinFileHeader* get_file_header() const { return file; }

		/**
		 * @brief Convert to string this file
		 * @return string value for this file
		 */
		std::string to_string() const
		{
			return std::format("#FBinFile <is-loaded: {0} file-header: {1}>",
				is_loaded, to_str(file));
		}
	private:

		/** The data block in the memory */
		FBinFileHeader* file;
		/** Loading state */
		bool is_loaded;
	};



}