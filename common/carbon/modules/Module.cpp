#include "common/carbon/modules/Module.hpp"
#include "common/carbon/files/BinaryFile.hpp"
#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/StateDesc.hpp"
#include "common/carbon/files/DciFile.hpp"
#include "files/FunctionDesc.hpp"
#include "lib/Variant.hpp"
#include <fmt/format.h>

using namespace carbon::files;
using namespace file_util;

namespace carbon::modules {


    // СТАРЫЙ КОНСТРУКТОР - адаптируем под новый API
    Module::Module(StringId module_name, std::filesystem::path path, std::vector<u8> binary_mem)
        : name(module_name), file_path(path), binary_file()
    {
        set_file(std::move(binary_mem));
    }

    bool Module::load_file()
    {
        return  false;
    }


    void Module::set_file(std::vector<u8> binary_mem) {
        this->binary_mem = std::move(binary_mem);
        if (this->binary_mem.empty()) {
            this->binary_file = nullptr;
            load_state = LoadState::METADATA;
        }
        else {
            this->binary_file = reinterpret_cast<BinaryFile*>(this->binary_mem.data());
            this->binary_file->relocate_pointers(true, this);
            build_export_table();
            load_state = LoadState::BINARY_LOADED;
        }
    }

    void Module::build_export_table() {
        if (!is_binary_loaded()) return;
        for (u32 i = 0; i < binary_file->definitions_count; i++) {
            auto def = binary_file->get_definition(i);
            if (def->has_flag(SymbolFlags::Export))
                add_export(def->name, def);
        }
    }
    Module::~Module() {
        binary_mem.clear();
        binary_file = nullptr;
        dci_binary_size = 0;
        file_path.clear();
        export_table.clear();
        import_table.clear();
    }

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
            auto def = binary_file->find_definition_by_name(name);
            if (def)
                return def;
        }

        // 2. Ищем в своих экспортах
        if (auto def = get_export(name)) {
            if (binary_file && binary_file->is_valid()) {
                // НОВЫЙ API - data_ptr уже Ptr<FunctionDesc>
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

    FunctionDesc* Module::resolve_function(StringId name) {
        auto definition = resolve_symbol(name);
        if (definition && definition->type == TypeIds::function)
            return (FunctionDesc*)definition->ptr.get();
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
        return fmt::format("Module('{}', state:{}, exports:{}, imports:{}, gen:{})",
            name,
            static_cast<int>(load_state),
            export_table.size(),
            import_table.size(),
            generation);
    }

    std::string Module::inspect() const {
        std::string result = fmt::format("Module[{}]\n", name);
        result += fmt::format("  File: {}\n", file_path.string());
        result += fmt::format("  Load state: {}\n", load_state_to_string(load_state));
        result += fmt::format("  Generation: {}, Load order: {}\n", generation, load_order);
        result += fmt::format("  DciBinary size: {} bytes\n", dci_binary_size);


        // Импорты
        result += fmt::format("  Imports: {} symbols\n", dci_imports.size());
        for (size_t i = 0; i < dci_imports.size() && i < 5; i++) { // показываем первые 5
            result += fmt::format("    - {}\n", dci_imports[i]);
        }
        if (dci_imports.size() > 5) {
            result += fmt::format("    ... and {} more\n", dci_imports.size() - 5);
        }

        // Экспорты
        result += fmt::format("  Exports: {} symbols\n", dci_exports.size());
        for (size_t i = 0; i < dci_exports.size() && i < 5; i++) {
            result += fmt::format("    - {}\n", dci_exports[i]);
        }
        if (dci_exports.size() > 5) {
            result += fmt::format("    ... and {} more\n", dci_exports.size() - 5);
        }

        // Таблица экспорта
        result += fmt::format("  Export table: {} entries\n", export_table.size());
        result += fmt::format("  Import table: {} modules\n", import_table.size());

        if (binary_file) {
            result += fmt::format("  Binary: {}", binary_file->inspect());
        }
        else {
            result += "  Binary: not loaded\n";
        }

        return result;
    }

    bool Module::save_to_binary_file(const std::string& path) const { 
          if (binary_file) 
              return binary_file->save_file(path);
          lg::error("save binary file with empty pointer");
          return false;
    };

    bool Module::save_to_dci_file(const std::string& path) const { 
        // Сохраняем .dci
        DciFile dci;
        dci.logical_path = name.to_string();
        dci.module_name = name.to_string();
        dci.binary_size = binary_mem.size();
        dci.imports = dci_imports;
        dci.exports = dci_exports;

        return dci.save(path);
    };

    // common/carbon/modules/Module.cpp - добавить:
    bool Module::save_to_files(const std::filesystem::path& base_path) const {
    // Берем только имя файла из module->name (последнюю часть)
    fs::path name_path(name.to_string());
    std::string filename = name_path.filename().string();  // "math"
    
    // Формируем полный путь
    fs::path full_path = fs::path(base_path) / filename;  // "modules/lib/math"
    
    // Создаем директорию
    fs::create_directories(full_path.parent_path());
    
    // Сохраняем .bin
    fs::path bin_path = full_path;
    bin_path += ".bin";
    if (!save_to_binary_file(bin_path.string())) {
        return false;
    }
    
    // Сохраняем .dci
    fs::path dci_path = full_path;
    dci_path += ".dci";
    return save_to_dci_file(dci_path.string());
    }

    TypeDesc* Module::resolve_type(StringId name) {
        auto def = resolve_symbol(name);
        if (def && def->type == TypeIds::type) {
            return reinterpret_cast<TypeDesc*>(def->ptr.get());
        }
        return nullptr;
    }
    
    // Вспомогательные функции для работы с TypeDesc в Module
    inline FunctionDesc* Module::resolve_method(StringId type_name, StringId method_name) {
        auto type_def = resolve_type(type_name);
        if (!type_def) return nullptr;
        
        return type_def->get_method(method_name);
    }

    inline StateDesc* Module::resolve_state(StringId type_name, StringId state_name) {
        auto type_def = resolve_type(type_name);
        if (!type_def) return nullptr;
        
        return type_def->resolve_state(state_name);
    }
    
    // Найти тип по имени с учётом иерархии
    TypeDesc* Module::find_type(StringId name) {
        auto type = resolve_type(name);
        if (type) return type;
        
        // Ищем в импортах
        for (auto& [import_name, imported_module] : import_table) {
            if (auto t = imported_module->resolve_type(name)) {
                return t;
            }
        }
        
        // Ищем в родительском модуле
        //if (parent_module && parent_module != this) {
        //    return parent_module->find_type(name);
        //}
        
        return nullptr;
    }

} // namespace vm