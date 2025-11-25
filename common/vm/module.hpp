#pragma once

#include "types.hpp"
#include "binary_file.hpp"  // ← ТЕПЕРЬ БЕЗОПАСНО - нет цикла
#include "util/log.h"
#include <memory>
#include <unordered_map>
#include <filesystem>

namespace vm {

    struct ByteCode;

    class Module {
    public:
        enum class LoadState {
            METADATA,       // Только метаданные из DCI
            BINARY_LOADED,  // + бинарные данные в пуле  
            LINKED          // + разрешены зависимости
        };

        // Identity
        StringId name = 0;
        std::filesystem::path file_path;

        // Metadata
        std::vector<StringId> imports;
        std::vector<StringId> exports;
        u32 binary_size = 0;

        // Runtime state
        LoadState load_state = LoadState::METADATA;
        u32 generation = 0;
        u32 load_order = 0;

        // Binary data - ТЕПЕРЬ СЫРОЙ УКАЗАТЕЛЬ!
        BinaryFile* binary_file = nullptr;

        // Linking data
        std::unordered_map<StringId, Definition*> export_table;
        std::unordered_map<StringId, std::shared_ptr<Module>> import_table;

    public:
        Module() = default;

        Module(StringId full_name, StringId short_name, std::filesystem::path file_path)
            : name(full_name), file_path(std::move(file_path)) {
        }

        // СТАРЫЙ КОНСТРУКТОР - адаптируем под новый API
        Module(StringId module_name, std::unique_ptr<BinaryFile> binary_file)
            : name(module_name) {
            if (binary_file) {
                // Для тестов - создаем копию в куче
                this->binary_file = new BinaryFile(*binary_file);
                load_state = LoadState::BINARY_LOADED;
            }
        }

        ~Module() {
            // Чистим только если НЕ из пула
             binary_file = nullptr;
        }

        bool is_valid_metadata() const { return name != 0 && !file_path.empty(); }
        bool is_binary_loaded() const { return load_state >= LoadState::BINARY_LOADED && binary_file != nullptr; }
        bool is_linked() const { return load_state >= LoadState::LINKED; }

        // Export management
        bool has_export(StringId name) const { return export_table.count(name) > 0; }
        Definition* get_export(StringId name) const;
        void add_export(StringId name, Definition* def) { export_table[name] = def; }

        // Import management  
        void add_import(StringId symbol_name, std::shared_ptr<Module> module);
        std::shared_ptr<Module> get_import(StringId symbol_name) const;

        // Symbol resolution - АДАПТИРУЕМ ПОД НОВЫЙ API
        Definition* resolve_symbol(StringId name);
        Definition* resolve_export(StringId name);
        Definition* resolve_symbol(StringId name, StringId type);
        ByteCode* resolve_code(StringId name);

        // Callback для пула при релокации
        void on_pool_relocation(BinaryFile* file_base) {
            binary_file = file_base;
            generation++;
            if (binary_file)
                binary_file->relocate_pointers(BinaryFilePool::get_base_address());
            lg::debug("Module {} relocated to new file base", string_id::to_string(name));
        }

        std::string to_string() const;

        std::string inspect() const {
            std::string result = std::format("Module[{}]\n", string_id::to_string(name));
            result += std::format("  File: {}\n", file_path.string());
            result += std::format("  Load state: {}\n", load_state_to_string(load_state));
            result += std::format("  Generation: {}, Load order: {}\n", generation, load_order);
            result += std::format("  Binary size: {} bytes\n", binary_size);


            // Импорты
            result += std::format("  Imports: {} symbols\n", imports.size());
            for (size_t i = 0; i < imports.size() && i < 5; i++) { // показываем первые 5
                result += std::format("    - {}\n", string_id::to_string(imports[i]));
            }
            if (imports.size() > 5) {
                result += std::format("    ... and {} more\n", imports.size() - 5);
            }

            // Экспорты
            result += std::format("  Exports: {} symbols\n", exports.size());
            for (size_t i = 0; i < exports.size() && i < 5; i++) {
                result += std::format("    - {}\n", string_id::to_string(exports[i]));
            }
            if (exports.size() > 5) {
                result += std::format("    ... and {} more\n", exports.size() - 5);
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
    private:
        static std::string load_state_to_string(LoadState state) {
            switch (state) {
            case LoadState::METADATA: return "METADATA";
            case LoadState::BINARY_LOADED: return "BINARY_LOADED";
            case LoadState::LINKED: return "LINKED";
            default: return "UNKNOWN";
            }
        }
    };

} // namespace vm