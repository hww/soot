#include "module.hpp"
#include "util/log.h"

namespace vm {

    Definition* Module::get_export(StringId name) const {
        auto it = export_table.find(name);
        return it != export_table.end() ? it->second : nullptr;
    }

    void Module::add_import(StringId symbol_name, std::shared_ptr<Module> module) {
        import_table[symbol_name] = std::move(module);
    }

    std::shared_ptr<Module> Module::get_import(StringId symbol_name) const {
        auto it = import_table.find(symbol_name);
        return it != import_table.end() ? it->second : nullptr;
    }

    Definition* Module::resolve_symbol(StringId name) {
        // 1. Ищем в бинарном файле
        if (binary_file) {
            binary_file->find_definition_by_name(name);
        }

        // 2. Ищем в своих экспортах
        if (auto def = get_export(name)) {
            if (binary_file && binary_file->is_valid()) {
                // НОВЫЙ API - data_ptr уже Ptr<ByteCode>
                return def;
            }
        }

        // 3. Ищем в импортах
        if (auto import_module = get_import(name)) {
            return import_module->resolve_export(name);
        }

        return nullptr;
    }

    Definition* Module::resolve_symbol(StringId name, StringId type) {
        auto definition = resolve_symbol(name);
        if (definition && definition->type == type)
            return definition;
        return nullptr;
    }

    ByteCode* Module::resolve_code(StringId name) {
        auto definition = resolve_symbol(name);
        if (definition && definition->type == type::function)
            return (ByteCode*)definition->data_ptr.c();
        return nullptr;
    }

    Definition* Module::resolve_export(StringId name) {
        // Ищем ТОЛЬКО в своих экспортах
        if (auto def = get_export(name)) {
            if (binary_file && binary_file->is_valid()) {
                return def;
            }
        }
        return nullptr;
    }

    std::string Module::to_string() const {
        return std::format("Module('{}', state:{}, exports:{}, imports:{}, gen:{})",
            string_id::to_string(name),
            static_cast<int>(load_state),
            export_table.size(),
            import_table.size(),
            generation);
    }

} // namespace vm