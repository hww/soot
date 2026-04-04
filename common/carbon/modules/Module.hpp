#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "util/Log.hpp"
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <vector>

using namespace carbon::lib;
using namespace carbon::files;

namespace carbon::modules {

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
        BinaryFile* binary_file;
        std::vector<u8> binary_mem;        

        // Linking data
        std::unordered_map<StringId, Definition*> export_table;
        std::unordered_map<StringId, std::shared_ptr<Module>> import_table;

    public:
        Module() = default;

        Module(StringId name, std::filesystem::path file_path)
            : name(name), file_path(std::move(file_path)) {
        }

        Module(StringId name, std::filesystem::path, std::vector<u8> binary_file);

        virtual ~Module();

        bool load_file();
        void build_export_table();
        void set_file(std::vector<u8> binary_mem);

        bool is_valid_metadata() const { return name.value != 0 && !file_path.empty(); }
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
        FunctionDesc* resolve_function(StringId name);
        FunctionDesc* resolve_method(StringId type_name, StringId method_name);
        StateDesc* resolve_state(StringId type_name, StringId state_name);
        TypeDesc* resolve_type(StringId name);
        TypeDesc* find_type(StringId name);

        std::string to_string() const;
        std::string inspect() const;

        bool save_to_binary_file(const std::string& path) const;
        bool save_to_dci_file(const std::string& path) const;
        bool save_to_files(const std::filesystem::path& base_path = ".") const;

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