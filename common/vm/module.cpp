#include "module.hpp"
#include "util/log.h"

namespace vm {

    Module::Module(StringId module_name, std::unique_ptr<BinaryFile> binary_file)
        : name(module_name), binary(std::move(binary_file)) {

        // Build exports table from binary definitions
        if (binary && binary->is_loaded()) {
            auto header = binary->get_header();
            for (u32 i = 0; i < header->defs_count; i++) {
                auto def = header->get_definition(i);
                exports[def->name] = def;
            }
        }
    }

    ByteCode* Module::resolve_symbol(StringId name) {
        auto it = exports.find(name);
        if (it != exports.end()) {
            return binary->get_header()->get_definition_ptr<ByteCode>(it->second->data_ptr.offset).c();
        }
        return nullptr;
    }

    Definition* Module::find_export(StringId name) const {
        auto it = exports.find(name);
        return it != exports.end() ? it->second : nullptr;
    }
    
    Definition* Module::find_export(StringId name, StringId type) const {
        auto it = exports.find(name);
        if (it != exports.end())
            return nullptr;
        if (it->second->type == type)
            return it->second;
        return nullptr;
    }

    ByteCode* Module::find_function(StringId name) const {
        auto item = find_export(name, SID("function"));
        return (item == nullptr) ? nullptr : (ByteCode*)item->data_ptr.c();
    }


    std::string Module::to_string() const {
        return std::format("Module('{}', exports:{}, deps:{}, valid:{})",
            string_id::to_string(name), exports.size(), dependencies.size(), is_valid());
    }

} // namespace vm