#include "module_hub.hpp"
#include "binary_file.hpp"
#include "util/assert.h"

namespace vm {

    std::shared_ptr<Module> GlobalModuleHub::load_module(StringId module_name) {
        // Check if already loaded
        auto existing = find_module(module_name);
        if (existing) {
            lg::debug("Module '{}' already loaded", string_id::to_string(module_name));
            return existing;
        }

        // Find file path
        auto file_path = registry_.find_module_file(module_name);
        if (file_path.empty()) {
            lg::error("Cannot find module file for: {}", string_id::to_string(module_name));
            return nullptr;
        }

        return load_module_internal(module_name, file_path);
    }

    std::shared_ptr<Module> GlobalModuleHub::load_file(const std::filesystem::path& file_path) {
        if (!std::filesystem::exists(file_path)) {
            lg::error("File not found: {}", file_path.string());
            return nullptr;
        }

        // Extract module name from filename
        std::string stem = file_path.stem().string();
        StringId module_name = string_id::register_string(stem);

        return load_module_internal(module_name, file_path);
    }

    std::shared_ptr<Module> GlobalModuleHub::load_module_internal(StringId module_name, const std::filesystem::path& file_path) {
        try {
            // Load binary file
            auto binary = std::make_unique<BinaryFile>();
            if (!binary->load_from_file(file_path.string())) {
                lg::error("Failed to load binary file: {}", file_path.string());
                return nullptr;
            }

            // Create module
            auto module = std::make_shared<Module>(module_name, std::move(binary));
            module->file_path = file_path;
            module->load_order = next_load_order_++;

            // Resolve dependencies
            resolve_dependencies(module.get());

            // Store module
            modules_[module_name] = module;

            lg::info("Loaded module: {}", module->to_string());
            return module;

        }
        catch (const std::exception& e) {
            lg::error("Exception loading module '{}': {}",
                string_id::to_string(module_name), e.what());
            return nullptr;
        }
    }

    std::shared_ptr<Module> GlobalModuleHub::find_module(StringId module_name) const {
        auto it = modules_.find(module_name);
        return it != modules_.end() ? it->second : nullptr;
    }

    ByteCode* GlobalModuleHub::find_export(StringId module_name, StringId export_name) {
        auto module = find_module(module_name);
        if (!module) {
            lg::error("Module not loaded: {}", string_id::to_string(module_name));
            return nullptr;
        }

        return module->resolve_symbol(export_name);
    }

    void GlobalModuleHub::resolve_dependencies(Module* module) {
        // TODO: Parse import metadata from binary file
        // For now, just log that we would resolve dependencies
        lg::debug("Resolving dependencies for module: {}", string_id::to_string(module->name));

        // This would recursively load dependent modules
        for (const auto& dep : module->dependencies) {
            if (!is_module_loaded(dep)) {
                lg::debug("Loading dependency: {} -> {}",
                    string_id::to_string(module->name),
                    string_id::to_string(dep));
                load_module(dep);
            }
        }
    }

    std::vector<StringId> GlobalModuleHub::get_loaded_modules() const {
        std::vector<StringId> result;
        for (const auto& [name, module] : modules_) {
            result.push_back(name);
        }
        return result;
    }

    bool GlobalModuleHub::is_module_loaded(StringId module_name) const {
        return modules_.find(module_name) != modules_.end();
    }

    void GlobalModuleHub::unload_module(StringId module_name) {
        auto it = modules_.find(module_name);
        if (it != modules_.end()) {
            lg::info("Unloading module: {}", string_id::to_string(module_name));
            modules_.erase(it);
        }
    }

    void GlobalModuleHub::reload_module(StringId module_name) {
        auto old_module = find_module(module_name);
        if (!old_module) {
            lg::warn("Cannot reload non-loaded module: {}", string_id::to_string(module_name));
            return;
        }

        auto file_path = old_module->file_path;
        unload_module(module_name);
        load_module_internal(module_name, file_path);
    }

    void GlobalModuleHub::link_module(Module* module) {
        // Предположим, что у модуля есть метод get_imported_symbols()
        // который возвращает имена символов которые он хочет импортировать
        auto import_requests = module->get_imported_symbols();

        for (StringId import_name : import_requests) {
            // Ищем модуль который экспортирует этот символ
            auto target_module = find_module_that_exports(import_name);
            if (target_module) {
                // Линкуем: символ → модуль
                module->add_import(import_name, target_module);
                lg::debug("Linked import: {} -> {}",
                    string_id::to_string(import_name),
                    string_id::to_string(target_module->name));
            }
            else {
                lg::error("Unresolved import: {}", string_id::to_string(import_name));
            }
        }
    }

    std::shared_ptr<Module> GlobalModuleHub::find_module_that_exports(StringId symbol_name) {
        // Ищем во ВСЕХ загруженных модулях
        for (auto& [module_name, module] : modules_) {
            if (module->has_export(symbol_name)) {
                return module;
            }
        }
        return nullptr;
    }

    // Новая функция: загрузка + линковка
    std::shared_ptr<Module> GlobalModuleHub::load_and_link_module(StringId module_name) {
        auto module = load_module(module_name);
        if (module) {
            link_module(module.get());
        }
        return module;
    }
    void GlobalModuleHub::build_dependency_graph(Module* module)
    {

    }
} // namespace vm