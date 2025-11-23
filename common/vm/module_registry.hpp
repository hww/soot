#pragma once

#include "types.hpp"
#include "string_id.hpp"
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <fstream>
#include <filesystem>

namespace vm {

    class ModuleRegistry {
    private:
        std::vector<std::filesystem::path> search_paths_;
        std::unordered_map<StringId, std::filesystem::path> path_cache_;

    public:
        static ModuleRegistry& instance() {
            static ModuleRegistry instance;
            return instance;
        }

        void add_search_path(const std::filesystem::path& path);
        std::filesystem::path find_module_file(StringId module_name);
        void clear_cache() { path_cache_.clear(); }

        const auto& get_search_paths() const { return search_paths_; }
    };

} // namespace vm