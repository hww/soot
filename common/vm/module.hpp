#pragma once

#include "types.hpp"
#include "binary_file.hpp"
#include <unordered_map>
#include <memory>
#include <filesystem>

namespace vm {

    class Module {
    public:
        // Identity
        StringId name;
        u32 generation{ 0 };
        std::filesystem::path file_path;

        // Core data
        std::unique_ptr<BinaryFile> binary;

        // Exports/Imports
        std::unordered_map<StringId, Definition*> exports;
        std::unordered_map<StringId, std::shared_ptr<Module>> imports; // local_name -> module_name
        std::vector<StringId> dependencies;

        // State
        bool is_initialized{ false };
        u32 load_order{ 0 };

    public:
        Module(StringId module_name, std::unique_ptr<BinaryFile> binary_file);

        // Basic methods
        ByteCode* resolve_symbol(StringId name);
        Definition* find_export(StringId name) const;
        Definition* find_export(StringId name, StringId type) const;
        ByteCode* find_function(StringId name) const;

        bool has_export(StringId name) const { return exports.count(name) > 0; }

        void add_dependency(StringId module_name) { dependencies.push_back(module_name); }
        void add_export(StringId name, Definition* def) { exports[name] = def; }
        void add_import(StringId symbol_name, std::shared_ptr<Module> module);

        std::string to_string() const;
        bool is_valid() const { return binary && binary->is_loaded(); }

        ByteCode* resolve_export(StringId name);
    private:
        ByteCode* get_bytecode_from_definition(Definition* def) {
            return binary->get_header()->get_definition_ptr<ByteCode>(def->data_ptr.offset).c();
        }
    };

} // namespace vm