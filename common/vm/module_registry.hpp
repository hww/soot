#pragma once

#include "types.hpp"
#include "binary_file.hpp"
#include "util/log.h"
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace vm {

    class ModuleRegistry {
    private:
        std::vector<std::filesystem::path> search_paths_;
        std::unordered_map<StringId, std::filesystem::path> module_index_;  // module_name -> file_path
        std::unordered_set<std::filesystem::path> scanned_paths_;
        bool is_index_dirty_ = true;

    public:
        static ModuleRegistry& instance() {
            static ModuleRegistry instance;
            return instance;
        }

        // Основной API
        void add_search_path(const std::filesystem::path& path);
        std::filesystem::path find_module_file(StringId module_name);
        std::vector<StringId> get_available_modules();

        // Сканирование и индексация
        void scan_and_index(bool force_rescan = false);
        void clear_cache();

        // Утилиты
        bool is_module_available(StringId module_name) const;
        std::string to_string() const;

    private:
        void scan_directory(const std::filesystem::path& directory);
        StringId extract_module_name_from_file(const std::filesystem::path& file_path);
        bool is_module_file(const std::filesystem::path& file_path);
    };

} // namespace vm