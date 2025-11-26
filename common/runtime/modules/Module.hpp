#pragma once

#include "common/runtime/ForwardDeclarations.hpp"
#include "common/runtime/lib/Types.hpp"
#include "common/runtime/lib/StringId.hpp"
#include "common/util/Log.hpp"
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <vector>

using namespace runtime::lib;
using namespace runtime::files;

namespace runtime::modules {

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
        std::vector<StringId> dci_imports;
        std::vector<StringId> dci_exports;
        u32 dci_binary_size = 0;

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

        Module(StringId name, std::filesystem::path file_path)
            : name(name), file_path(std::move(file_path)) {
        }

        // СТАРЫЙ КОНСТРУКТОР - адаптируем под новый API
        Module(StringId module_name, std::unique_ptr<BinaryFile> binary_file);

        ~Module();

        bool is_valid_metadata() const { return name != 0 && !file_path.empty(); }
        bool is_linked() const { return load_state >= LoadState::LINKED; }
        virtual bool is_binary_loaded() const { return load_state >= LoadState::BINARY_LOADED && binary_file != nullptr; }

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
        void on_pool_relocation(BinaryFile* file_base);

        void on_pool_deaelocation(BinaryFile* file_base);

        std::string to_string() const;

        std::string inspect() const;
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

    class RootModule : public Module {
        RootModule() : Module(SID("::root"), "::root") {
            // Специальный модуль без файла
            load_state = LoadState::LINKED;  // сразу в связанном состоянии
        }

        // Не имеет бинарного файла
        bool is_binary_loaded() const override { return true; }
    };
} // namespace vm