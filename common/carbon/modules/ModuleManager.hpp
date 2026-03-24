#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/carbon/modules/ModuleRegistry.hpp"
#include <memory>
#include <unordered_map>

namespace carbon::modules {

    class ModuleManager {
    private:
        ModuleRegistry& registry_;
        std::unordered_map<StringId, std::shared_ptr<Module>> loaded_modules_;
        std::unordered_map<StringId, size_t> reference_counts_;
        u32 next_load_order_ = 0;

    public:
        ModuleManager() : registry_(ModuleRegistry::instance()) {}

        static ModuleManager& instance() {
            static ModuleManager instance;
            return instance;
        }

        // Основной API
        std::shared_ptr<Module> load_module(StringId module_name);
        void unload_module(StringId module_name);
        void hot_reload_module(StringId module_name);

        // Дополнительные методы
        std::shared_ptr<Module> load_file(const std::filesystem::path& file_path);
        std::shared_ptr<Module> find_module(StringId module_name) const;
        Definition* find_export(StringId module_name, StringId export_name);

        // Информация
        std::vector<StringId> get_loaded_modules() const;
        bool is_module_loaded(StringId module_name) const;
        u32 get_module_generation(StringId module_name) const;
        size_t get_module_reference_count(StringId module_name) const;

    private:
        std::shared_ptr<Module> load_module_internal(StringId module_name);
        bool load_binary_data(const std::shared_ptr<Module>& module);
        void build_export_table(const std::shared_ptr<Module>& module);
        void resolve_dependencies(Module* module);
        void link_module(Module* module);
        std::shared_ptr<Module> find_module_that_exports(StringId symbol_name);
    };

} // namespace vm