#include "module_registry.hpp"
#include "util/assert.h"
#include <fstream>
#include <algorithm>

namespace vm {

    void ModuleRegistry::add_search_path(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            lg::warn("Search path does not exist: {}", path.string());
            return;
        }

        auto canonical_path = std::filesystem::canonical(path);

        // Проверяем дубликаты
        if (std::find(search_paths_.begin(), search_paths_.end(), canonical_path) != search_paths_.end()) {
            lg::debug("Search path already added: {}", canonical_path.string());
            return;
        }

        search_paths_.push_back(canonical_path);
        is_index_dirty_ = true;

        lg::info("Added module search path: {}", canonical_path.string());
    }

    std::filesystem::path ModuleRegistry::find_module_file(StringId module_name) {
        if (is_index_dirty_) {
            scan_and_index();
        }

        auto it = module_index_.find(module_name);
        if (it != module_index_.end()) {
            return it->second;
        }

        lg::debug("Module not found: {}", string_id::to_string(module_name));
        return "";
    }

    void ModuleRegistry::scan_and_index(bool force_rescan) {
        if (!force_rescan && !is_index_dirty_) {
            return;
        }

        lg::info("Scanning for modules in {} search paths", search_paths_.size());

        module_index_.clear();
        scanned_paths_.clear();

        for (const auto& search_path : search_paths_) {
            scan_directory(search_path);
        }

        is_index_dirty_ = false;
        lg::info("Indexed {} modules", module_index_.size());
    }

    void ModuleRegistry::scan_directory(const std::filesystem::path& directory) {
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            return;
        }

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (!entry.is_regular_file()) continue;

                const auto& file_path = entry.path();
                if (!is_module_file(file_path)) continue;

                // Парсим файл чтобы извлечь имя модуля
                StringId module_name = extract_module_name_from_file(file_path);
                if (module_name != StringId(0)) {
                    module_index_[module_name] = file_path;
                    lg::debug("Found module: {} -> {}", string_id::to_string(module_name), file_path.string());
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            lg::error("Filesystem error scanning {}: {}", directory.string(), e.what());
        }
    }

    StringId ModuleRegistry::extract_module_name_from_file(const std::filesystem::path& file_path) {
        try {
            // Пытаемся загрузить бинарный файл и прочитать имя модуля из заголовка
            auto binary = std::make_unique<BinaryFile>();
            if (!binary->load_from_file(file_path.string())) {
                return StringId(0);
            }

            auto header = binary->get_header();

            // Предположим, что имя модуля хранится в первой дефиниции
            // или в специальном поле заголовка
            if (header->defs_count > 0) {
                auto first_def = header->get_definition(0);
                return first_def->name;  // Или другая логика извлечения имени
            }

        }
        catch (const std::exception& e) {
            lg::error("Error parsing module file {}: {}", file_path.string(), e.what());
        }

        return StringId(0);
    }

    bool ModuleRegistry::is_module_file(const std::filesystem::path& file_path) {
        static const std::unordered_set<std::string> valid_extensions = {
            ".dci", ".bin"
        };

        auto extension = file_path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        return valid_extensions.count(extension) > 0;
    }

    std::vector<StringId> ModuleRegistry::get_available_modules() {
        if (is_index_dirty_) {
            scan_and_index();
        }

        std::vector<StringId> modules;
        for (const auto& [module_name, path] : module_index_) {
            modules.push_back(module_name);
        }
        return modules;
    }

    bool ModuleRegistry::is_module_available(StringId module_name) const {
        return module_index_.find(module_name) != module_index_.end();
    }

    void ModuleRegistry::clear_cache() {
        module_index_.clear();
        scanned_paths_.clear();
        is_index_dirty_ = true;
        lg::info("Module registry cache cleared");
    }

    std::string ModuleRegistry::to_string() const {
        return std::format("ModuleRegistry(paths:{}, modules:{}, dirty:{})",
            search_paths_.size(), module_index_.size(), is_index_dirty_);
    }

} // namespace vm