#pragma once

#include "module.hpp"
#include "module_registry.hpp"
#include "util/log.h"
#include <memory>
#include <unordered_map>

namespace vm {

    class GlobalModuleHub {
    private:
        std::unordered_map<StringId, std::shared_ptr<Module>> modules_;
        ModuleRegistry& registry_;
        u32 next_load_order_{ 0 };

    public:
        GlobalModuleHub() : registry_(ModuleRegistry::instance()) {}

        static GlobalModuleHub& instance() {
            static GlobalModuleHub instance;
            return instance;
        }

        // Main API
        std::shared_ptr<Module> load_module(StringId module_name);
        std::shared_ptr<Module> load_file(const std::filesystem::path& file_path);

        // Resolution
        std::shared_ptr<Module> find_module(StringId module_name) const;
        ByteCode* find_export(StringId module_name, StringId export_name);

        // Dependency management
        void resolve_dependencies(Module* module);

        // Information
        std::vector<StringId> get_loaded_modules() const;
        bool is_module_loaded(StringId module_name) const;

        // Utility
        void unload_module(StringId module_name);
        void reload_module(StringId module_name);
        
        // Linking
        void link_module(Module* module);
    private:
        std::shared_ptr<Module> load_and_link_module(StringId module_name);
        std::shared_ptr<Module> find_module_that_exports(StringId symbol_name);
    private:
        std::shared_ptr<Module> load_module_internal(StringId module_name, const std::filesystem::path& file_path);
        void build_dependency_graph(Module* module);
    };

} // namespace vm