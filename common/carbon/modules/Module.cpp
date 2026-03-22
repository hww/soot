#include "common/carbon/modules/Module.hpp"
#include "common/carbon/files/BinaryFile.hpp"
#include "common/carbon/files/DciFile.hpp"
#include "common/util/Log.hpp"

using namespace runtime::files;

namespace runtime::modules {


    // СТАРЫЙ КОНСТРУКТОР - адаптируем под новый API
    Module::Module(StringId module_name, std::filesystem::path path, std::vector<u8> binary_mem)
        : name(module_name), file_path(path), binary_file()
    {
        set_file(std::move(binary_mem));
    }
    bool Module::load_file()
    {

    }

    void Module::set_file(std::vector<u8> binary_mem) {
        this->binary_mem = std::move(binary_mem);
        if (this->binary_mem.empty()) {
            this->binary_file = nullptr;
            load_state = LoadState::METADATA;
        }
        else {
            this->binary_file = reinterpret_cast<BinaryFile*>(this->binary_mem.data());
            this->binary_file->set_owner(this);
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

    // common/carbon/modules/Module.cpp - добавить:
    bool Module::save_to_files(const std::filesystem::path& base_path) const {
        // Сохраняем .bin
        std::filesystem::path bin_path = base_path / (lib::to_string(name) + ".bin");
        std::ofstream bin_file(bin_path, std::ios::binary);
        if (!bin_file) return false;

        // make pointers relative to files
        binary_file->relocate_pointers(false);

        bin_file.write(reinterpret_cast<const char*>(binary_mem.data()), binary_mem.size());
        bin_file.close();
        
		// restore pointers relative to memory
        binary_file->relocate_pointers(true);
		
        // Сохраняем .dci
        DciFile dci;
        dci.logical_path = lib::to_string(name);
        dci.module_name = lib::to_string(name);
        dci.binary_size = binary_mem.size();
        dci.imports = dci_imports;
        dci.exports = dci_exports;
        
        std::filesystem::path dci_path = base_path / (lib::to_string(name) + ".dci");
        return dci.save(dci_path.string());
    }

} // namespace vm