#include "module_registry.hpp"
#include "util/log.h"
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

    std::shared_ptr<Module> ModuleRegistry::find_module(StringId module_name) {
        if (is_index_dirty_) {
            scan_and_index();
        }

        auto it = module_cache_.find(module_name);
        if (it != module_cache_.end()) {
            return it->second;
        }

        lg::debug("Module not found in registry: {}", string_id::to_string(module_name));
        return nullptr;
    }

    std::vector<StringId> ModuleRegistry::get_available_modules() {
        if (is_index_dirty_) {
            scan_and_index();
        }

        std::vector<StringId> modules;
        for (const auto& [module_name, module] : module_cache_) {
            modules.push_back(module_name);
        }
        return modules;
    }

    void ModuleRegistry::scan_and_index(bool force_rescan) {
        if (!force_rescan && !is_index_dirty_) {
            return;
        }

        lg::info("Scanning for modules in {} search paths", search_paths_.size());

        module_cache_.clear();

        for (const auto& search_path : search_paths_) {
            scan_directory(search_path);
        }

        is_index_dirty_ = false;
        lg::info("Registry indexed {} modules", module_cache_.size());
    }

    void ModuleRegistry::scan_directory(const std::filesystem::path& directory) {
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            return;
        }

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (!entry.is_regular_file()) continue;

                const auto& file_path = entry.path();
                if (file_path.extension() != ".dci") continue;

                // Создаем модуль из DCI файла
                auto module = create_module_from_dci(file_path);
                if (module && module->is_valid_metadata()) {
                    module_cache_[module->name] = module;
                    lg::debug("Registered module: {} -> {}",
                        string_id::to_string(module->name), file_path.string());
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            lg::error("Filesystem error scanning {}: {}", directory.string(), e.what());
        }
    }

    std::shared_ptr<Module> ModuleRegistry::create_module_from_dci(const std::filesystem::path& dci_path) {
        try {
            // Парсим DCI файл
            DciFile dci = DciFile::parse(dci_path.string());
            if (!dci.is_valid()) {
                lg::warn("Invalid DCI file: {}", dci_path.string());
                return nullptr;
            }

            // ИСПРАВЛЕНИЕ: создаем модуль и инициализируем поля
            auto module = std::make_shared<Module>();

            // Заполняем поля напрямую
            module->name = string_id::register_string(dci.logical_path.c_str());
            module->file_path = dci_path;
            module->dci_imports = std::move(dci.imports);
            module->dci_exports = std::move(dci.exports);
            module->dci_binary_size = dci.binary_size;
            module->load_state = Module::LoadState::METADATA;

            // Находим соответствующий .bin файл
            std::filesystem::path bin_path = dci_path;
            bin_path.replace_extension(".bin");
            if (!std::filesystem::exists(bin_path)) {
                lg::warn("Binary file not found for module: {}", bin_path.string());
                // Но все равно возвращаем модуль - метаданные валидны
            }

            return module;

        }
        catch (const std::exception& e) {
            lg::error("Error parsing DCI file {}: {}", dci_path.string(), e.what());
            return nullptr;
        }
    }

    void ModuleRegistry::clear_cache() {
        module_cache_.clear();
        is_index_dirty_ = true;
        lg::info("Module registry cache cleared");
    }

    bool ModuleRegistry::is_module_available(StringId module_name) const {
        return module_cache_.find(module_name) != module_cache_.end();
    }

    std::string ModuleRegistry::to_string() const {
        return std::format("ModuleRegistry(paths:{}, modules:{}, dirty:{})",
            search_paths_.size(), module_cache_.size(), is_index_dirty_);
    }

} // namespace vm