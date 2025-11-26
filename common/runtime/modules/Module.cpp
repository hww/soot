#include "common/runtime/modules/Module.hpp"
#include "common/runtime/files/BinaryFile.hpp"
#include "common/runtime/files/BinaryFilePool.hpp"
#include "common/util/Log.hpp"

using namespace runtime::files;

namespace runtime::modules {


    // СТАРЫЙ КОНСТРУКТОР - адаптируем под новый API
    Module::Module(StringId module_name, std::unique_ptr<BinaryFile> binary_file)
        : name(module_name) {
        if (binary_file) {
            // Для тестов - создаем копию в куче
            this->binary_file = new BinaryFile(*binary_file);
            load_state = LoadState::BINARY_LOADED;
        }
    }

    Module::~Module() {
        // Чистим только если НЕ из пула
        BinaryFilePool::deallocate(name);
        binary_file = nullptr;
        dci_binary_size = 0;
        file_path.clear();
        export_table.clear();
        import_table.clear();
    }

    // Callback для пула при релокации
    void Module::on_pool_relocation(BinaryFile* file_base) {
        binary_file = file_base;
        generation++;
        if (binary_file)
            binary_file->relocate_pointers(BinaryFilePool::get_base_address());
        lg::debug("Module {} relocated to new file base", lib::to_string(name));
    }

    void Module::on_pool_deaelocation(BinaryFile* file_base) {
        binary_file = file_base;
        generation++;
        lg::debug("Module {} dealocated to new file base", lib::to_string(name));
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
            lib::to_string(name),
            static_cast<int>(load_state),
            export_table.size(),
            import_table.size(),
            generation);
    }

    std::string Module::inspect() const {
        std::string result = std::format("Module[{}]\n", lib::to_string(name));
        result += std::format("  File: {}\n", file_path.string());
        result += std::format("  Load state: {}\n", load_state_to_string(load_state));
        result += std::format("  Generation: {}, Load order: {}\n", generation, load_order);
        result += std::format("  DciBinary size: {} bytes\n", dci_binary_size);


        // Импорты
        result += std::format("  Imports: {} symbols\n", dci_imports.size());
        for (size_t i = 0; i < dci_imports.size() && i < 5; i++) { // показываем первые 5
            result += std::format("    - {}\n", lib::to_string(dci_imports[i]));
        }
        if (dci_imports.size() > 5) {
            result += std::format("    ... and {} more\n", dci_imports.size() - 5);
        }

        // Экспорты
        result += std::format("  Exports: {} symbols\n", dci_exports.size());
        for (size_t i = 0; i < dci_exports.size() && i < 5; i++) {
            result += std::format("    - {}\n", lib::to_string(dci_exports[i]));
        }
        if (dci_exports.size() > 5) {
            result += std::format("    ... and {} more\n", dci_exports.size() - 5);
        }

        // Таблица экспорта
        result += std::format("  Export table: {} entries\n", export_table.size());
        result += std::format("  Import table: {} modules\n", import_table.size());

        if (binary_file) {
            result += std::format("  Binary: {}\n", binary_file->inspect());
        }
        else {
            result += "  Binary: not loaded\n";
        }

        return result;
    }

} // namespace vm