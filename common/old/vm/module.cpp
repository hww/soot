#include "module.hpp"
#include "context.hpp"
#include "environment.hpp"
#include <filesystem>
#include <fstream>

namespace vm
{
	//--------------------------------------------------------------------------
	// Search files in the folders
	//--------------------------------------------------------------------------

	std::vector<std::filesystem::path> Module::search_path_{"." };

	void Module::add_search_path(std::filesystem::path path)
	{
		search_path_.push_back(path);
	}

	void Module::remove_search_path(std::filesystem::path path)
	{
		std::vector<std::filesystem::path>::iterator new_end;
		new_end = std::remove(search_path_.begin(), search_path_.end(), path);
	}

	bool Module::find_file(const std::string name, std::filesystem::path& resultPath)
	{
		std::filesystem::path filePath(name);
		// The file checking function
		auto exists_test = [](const std::string& name) {
			struct stat buffer {};
			return (stat(name.c_str(), &buffer) == 0);
		};
		const auto dciPath = filePath.replace_extension("dci");
		const auto binPath = filePath.replace_extension("bin");
		for (auto& t : search_path_)
		{
			// Verify the existence of DCI file
			if (!exists_test((t/dciPath).generic_string())) 
				continue;
			if (!exists_test((t/binPath).generic_string()))
				continue;
			resultPath = t;
			return true;
		}
		return false;
	}

	//--------------------------------------------------------------------------
	// The file constructor/destructor
	//--------------------------------------------------------------------------

	Module::~Module()
	{
		undef_file();
	}

	//--------------------------------------------------------------------------
	// The file constructor/destructor
	//--------------------------------------------------------------------------

	std::map<StringId, ModuleRef> Module::sFilesTable{};

	ModuleRef* Module::define_file()
	{
		sFilesTable[name] = ModuleRef(this, GetNextGeneration());
		return &sFilesTable[name];
	}

	void Module::undef_file() const
	{
		sFilesTable[name] = ModuleRef(nullptr, GetNextGeneration());
	}

	//--------------------------------------------------------------------------
	// The file constructor/destructor
	//--------------------------------------------------------------------------

	void Module::import(const std::string& name)
	{
		const auto definition = g_environment.lookup(define_string(name));
		if (definition == nullptr)
			LoadFile(name);
	}

	u32 Module::sScriptFileGeneration = 1;

	constexpr u32 Module::GetNextGeneration()
	{
		return ++sScriptFileGeneration;
	}

	void Module::LoadFile(const std::string& _name, EScriptLoadOptions options)
	{
		std::filesystem::path dir;
		if (!find_file(_name, dir))
			throw FRuntimeError(g_script_context, "LoadFile : The .bin and .dci files found", _name);

		auto binFile = dir / _name;
		auto dciFile = dir / _name;
		binFile = binFile.replace_extension("bin");
		dciFile = dciFile.replace_extension("dci");

		// Read and parse the source file
		std::ifstream dciStream(dciFile);
		info.load_file(dciStream);
		name = info.name;

		// Read all imported files
		if (options | EScriptImport)
		{
			for (auto& t : info.imports)
			{
				const auto importName = lookup_string(t);
				if (importName == NULL_STRING)
					throw FRuntimeError(g_script_context, "LoadFile : Can't find file", lookup_string_safe(t));
				import(importName);
			}
		}

		// Read and parse the bin file
		std::ifstream binStream(binFile, std::ios::binary | std::ios::in);
		bin.load_file(binStream);

		// Initialize local environment
		auto header = bin.get_file_header();
		assert(header != nullptr);
		for (size_t i = 0; i < header->defs_num; i++) {
			const auto definition = header->get_definition(i);
			const auto pointer = header->get_definition_ptr(i);
			env.define(definition->Name, FLocalDefinition{ definition->Type, pointer });
		}

		// Define the file 
		auto fileRef = define_file();

		// Read all imported files
		if (options | EScriptExport) {
			for (auto& exportName : info.exports)
			{
				auto definition = env.lookup(exportName);
				if (definition == nullptr)
					throw FRuntimeError(g_script_context, "Exported definition is not found", to_str(exportName));
				g_environment.define(exportName, FGlobalDefinition(definition->type, definition->ptr, fileRef));
			}
		}

	}

	std::string Module::to_string() const
	{
		return std::format("#ScriptFile <{0}>", to_str(name));
	}


}
