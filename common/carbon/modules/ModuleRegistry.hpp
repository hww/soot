#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/files/DciFile.hpp"
#include "common/carbon/modules/Module.hpp"
#include <filesystem>
#include <unordered_map>
#include <memory>

namespace carbon::modules {

    class ModuleRegistry {
    private:
        std::vector<std::filesystem::path> search_paths_;
        std::unordered_map<StringId, std::shared_ptr<Module>> module_cache_;
        bool is_index_dirty_ = true;

    public:
        static ModuleRegistry& instance() {
            static ModuleRegistry instance;
            return instance;
        }

        // Основной API (сохраняем старый интерфейс)
        void add_search_path(const std::filesystem::path& path);
        std::shared_ptr<Module> find_module(StringId module_name);
        std::vector<StringId> get_available_modules();
        void scan_and_index(bool force_rescan = false);
        void clear_cache();

        // Утилиты (старый API)
        bool is_module_available(StringId module_name) const;
        std::string to_string() const;

    private:
        void scan_directory(const std::filesystem::path& directory);
        std::shared_ptr<Module> create_module_from_dci(const std::filesystem::path& dci_path);
        std::filesystem::path find_dci_file(const std::filesystem::path& directory, StringId module_name);
    };

} // namespace vm