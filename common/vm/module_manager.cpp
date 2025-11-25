#include "module_manager.hpp"
#include "util/log.h"
#include "util/assert.h"
#include <fstream>

namespace vm {

    std::shared_ptr<Module> ModuleManager::load_module(StringId module_name) {
        // Проверяем уже загруженные модули
        auto existing = find_module(module_name);
        if (existing) {
            reference_counts_[module_name]++;
            lg::debug("Module '{}' already loaded, refcount: {}",
                string_id::to_string(module_name), reference_counts_[module_name]);
            return existing;
        }

        return load_module_internal(module_name);
    }

    std::shared_ptr<Module> ModuleManager::load_module_internal(StringId module_name) {
        // Получаем модуль из регистра
        auto module = registry_.find_module(module_name);
        if (!module) {
            lg::error("Module not found in registry: {}", string_id::to_string(module_name));
            return nullptr;
        }

        // Загружаем бинарные данные
        if (module->load_state == Module::LoadState::METADATA) {
            if (!load_binary_data(module)) {
                lg::error("Failed to load binary data for module: {}", string_id::to_string(module_name));
                return nullptr;
            }
        }

        // Разрешаем зависимости
        resolve_dependencies(module.get());

        // Линкуем модуль
        link_module(module.get());

        // Сохраняем и обновляем состояние
        module->load_order = next_load_order_++;
        loaded_modules_[module_name] = module;
        reference_counts_[module_name] = 1;

        lg::info("Successfully loaded module: {}", module->to_string());
        return module;
    }

    bool ModuleManager::load_binary_data(const std::shared_ptr<Module>& module) {
        ASSERT(module->load_state == Module::LoadState::METADATA);

        // Находим .bin файл
        std::filesystem::path bin_path = module->file_path;
        bin_path.replace_extension(".bin");

        if (!std::filesystem::exists(bin_path)) {
            lg::error("Binary file not found: {}", bin_path.string());
            return false;
        }

        try {
            std::ifstream file(bin_path, std::ios::binary);
            if (!file) {
                lg::error("Failed to open binary file: {}", bin_path.string());
                return false;
            }

            // 1. Узнаем размер файла
            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            // 2. Выделяем память в пуле
            void* memory = BinaryFilePool::allocate(
                static_cast<u32>(file_size),
                module.get(),
                module->name
            );
            if (!memory) {
                lg::error("Failed to allocate memory in pool for module: {}",
                    string_id::to_string(module->name));
                return false;
            }

            // 3. Читаем файл ПРЯМО в память пула
            file.read(static_cast<char*>(memory), file_size);

            // 4. Инициализируем BinaryFile
            BinaryFile* binary_file = static_cast<BinaryFile*>(memory);
            binary_file->relocate_pointers(BinaryFilePool::get_base_address());

            // 5. Сохраняем в модуле
            module->binary_file = binary_file;
            module->load_state = Module::LoadState::BINARY_LOADED;

            // 6. Строим таблицу экспортов
            build_export_table(module);

            lg::debug("Loaded binary data for module: {} (size: {} bytes, addr: {})",
                string_id::to_string(module->name), file_size, memory);
            return true;

        }
        catch (const std::exception& e) {
            lg::error("Exception loading binary data for '{}': {}",
                string_id::to_string(module->name), e.what());
            return false;
        }
    }

    void ModuleManager::build_export_table(const std::shared_ptr<Module>& module) {
        ASSERT(module->is_binary_loaded());

        BinaryFile* file = module->binary_file;
        for (u32 i = 0; i < file->definitions_count; i++) {
            auto def = file->get_definition(i);

            // Если символ в списке экспортов - добавляем в таблицу
            if (std::find(module->exports.begin(), module->exports.end(), def->name) != module->exports.end()) {
                module->add_export(def->name, def);
            }
        }

        lg::debug("Built export table for '{}': {} symbols",
            string_id::to_string(module->name), module->export_table.size());
    }

    void ModuleManager::resolve_dependencies(Module* module) {
        for (const auto& import_name : module->imports) {
            if (!is_module_loaded(import_name)) {
                lg::debug("Loading dependency: {} -> {}",
                    string_id::to_string(module->name),
                    string_id::to_string(import_name));

                if (!load_module_internal(import_name)) {
                    lg::error("Failed to load dependency: {} for module: {}",
                        string_id::to_string(import_name),
                        string_id::to_string(module->name));
                }
            }
        }
    }

    void ModuleManager::link_module(Module* module) {
        // Линкуем импорты
        for (const auto& import_name : module->imports) {
            auto target_module = find_module_that_exports(import_name);
            if (target_module) {
                module->add_import(import_name, target_module);
                lg::debug("Linked import: {} -> {} ({})",
                    string_id::to_string(module->name),
                    string_id::to_string(import_name),
                    string_id::to_string(target_module->name));
            }
            else {
                lg::error("Unresolved import: {} in module {}",
                    string_id::to_string(import_name),
                    string_id::to_string(module->name));
            }
        }

        module->load_state = Module::LoadState::LINKED;
        lg::debug("Successfully linked module: {}", string_id::to_string(module->name));
    }

    std::shared_ptr<Module> ModuleManager::find_module_that_exports(StringId symbol_name) {
        // Ищем во всех загруженных модулях
        for (auto& [module_name, module] : loaded_modules_) {
            if (module->has_export(symbol_name)) {
                return module;
            }
        }
        return nullptr;
    }

    void ModuleManager::unload_module(StringId module_name) {
        auto ref_it = reference_counts_.find(module_name);
        if (ref_it == reference_counts_.end()) return;

        ref_it->second--;
        lg::debug("Decremented refcount for module '{}': {}",
            string_id::to_string(module_name), ref_it->second);

        if (ref_it->second == 0) {
            auto mod_it = loaded_modules_.find(module_name);
            if (mod_it != loaded_modules_.end()) {
                // Выгружаем из BinaryFilePool
                BinaryFilePool::deallocate(module_name);
                lg::info("Unloaded module: {}", string_id::to_string(module_name));
            }
            loaded_modules_.erase(module_name);
            reference_counts_.erase(module_name);
        }
    }

    void ModuleManager::hot_reload_module(StringId module_name) {
        auto it = loaded_modules_.find(module_name);
        if (it == loaded_modules_.end()) {
            lg::error("Cannot hot reload - module not loaded: {}", string_id::to_string(module_name));
            return;
        }

        auto module = it->second;
        lg::info("Hot reloading module: {}", string_id::to_string(module_name));

        // 1. Выгружаем старую версию из пула
        BinaryFilePool::deallocate(module_name);

        // 2. Сбрасываем состояние модуля
        module->binary_file = nullptr;
        module->load_state = Module::LoadState::METADATA;
        module->export_table.clear();

        // 3. Загружаем новую версию
        if (load_binary_data(module)) {
            lg::info("Successfully hot reloaded module: {} -> generation {}",
                string_id::to_string(module_name), module->generation);
        }
        else {
            lg::error("Failed to hot reload module: {}", string_id::to_string(module_name));
            // Восстанавливаем предыдущее состояние?
        }
    }

    std::shared_ptr<Module> ModuleManager::find_module(StringId module_name) const {
        auto it = loaded_modules_.find(module_name);
        return it != loaded_modules_.end() ? it->second : nullptr;
    }

    Definition* ModuleManager::find_export(StringId module_name, StringId export_name) {
        auto module = find_module(module_name);
        if (!module) {
            lg::error("Module not loaded: {}", string_id::to_string(module_name));
            return nullptr;
        }

        return module->resolve_export(export_name);
    }

    std::shared_ptr<Module> ModuleManager::load_file(const std::filesystem::path& file_path) {
        // TODO: Реализовать загрузку модуля напрямую из файла
        // Создать временный Module, загрузить бинарные данные, но не регистрировать в ModuleRegistry
        lg::warn("load_file not implemented yet: {}", file_path.string());
        return nullptr;
    }

    std::vector<StringId> ModuleManager::get_loaded_modules() const {
        std::vector<StringId> result;
        result.reserve(loaded_modules_.size());
        for (const auto& [name, _] : loaded_modules_) {
            result.push_back(name);
        }
        return result;
    }

    bool ModuleManager::is_module_loaded(StringId module_name) const {
        return loaded_modules_.find(module_name) != loaded_modules_.end();
    }

    u32 ModuleManager::get_module_generation(StringId module_name) const {
        auto module = find_module(module_name);
        return module ? module->generation : 0;
    }

    size_t ModuleManager::get_module_reference_count(StringId module_name) const {
        auto it = reference_counts_.find(module_name);
        return it != reference_counts_.end() ? it->second : 0;
    }

} // namespace vm