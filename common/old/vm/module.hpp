#pragma once

#include <filesystem>
#include "module_bin.hpp"
#include "module_info.hpp"
#include "string_id.hpp"
#include "platform.hpp"
#include "environment.hpp"

namespace vm
{
	enum  EScriptLoadOptions { EScriptImport = 1 << 0, EScriptExport = 1 << 1 };

	ENUM_FLAG_OPERATORS(EScriptLoadOptions);

	class Module;


	struct ModuleRef
	{
		ModuleRef() = default;
		ModuleRef(Module* _module, u32 _generation) : module_ref(_module), generation(_generation){}

		ModuleRef& operator=(const ModuleRef& other) = default;
		std::string ToString() const { return std::format("#ModuleRef <this: {0:X} module: {1:X} generation: {2}>", (PTRINT)this, (PTRINT)module_ref,generation); }

		Module* module_ref;
		u32 generation;
	};

	std::string to_str(const Module* obj);

	class Module
	{
	public:
		/**/
		Module() : name(), bin(), info() {}
		/**/
		~Module();
		/**
		 * @brief Define all file content
		 * @return The pointer to the module
		*/
		ModuleRef* define_file();
		/**
		 * @brief Undefine the file content
		*/
		void undef_file() const;
		/**
		 * @brief Import file in two steps, find in memory and then
		 * if the file is not found search on the disk
		 * @param name - The definition name (file name)
		 */
		void import(const std::string& name);
		/**
		 * @brief Load the script file by name
		 * @param name - The definition name (file name)
		 * @param options - The loading mode
		 */
		void LoadFile(const std::string& name, EScriptLoadOptions options = EScriptImport | EScriptExport);
		/**
		 * @brief  Convert to a string
		 * @return String value
	  	 */
		std::string to_string() const;
		/**
		 * @brief Add search path to the system
		 * @param path - Path to the folder
		 */
		static void add_search_path(std::filesystem::path path);
		/**
		 * @brief Remove search path to the system
		 * @param path - Path to the folder
		 */
		static void remove_search_path(std::filesystem::path path);

		StringId name;
		FBinFile bin;
		FModuleInfo info;
		FLocalEnvironment env;

	private:
		bool find_file(std::string name, std::filesystem::path& resultPath);
		static std::vector<std::filesystem::path> search_path_;

		static std::map<StringId, ModuleRef> sFilesTable;
		static u32 sScriptFileGeneration;
		static constexpr u32 GetNextGeneration();
	};


}
