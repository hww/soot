#include "module_registry.hpp"
#include "string_id.hpp"
#include "util/assert.h"
#include "util/log.h"


namespace vm {

    void ModuleRegistry::add_search_path(const std::filesystem::path& path) {
        if (std::filesystem::exists(path)) {
            search_paths_.push_back(std::filesystem::canonical(path));
            lg::info("Added module search path: {}", search_paths_.back().string());
        }
        else {
            lg::warn("Module search path does not exist: {}", path.string());
        }
    }

    std::filesystem::path ModuleRegistry::find_module_file(StringId module_name) {
        // Check cache first
        auto cache_it = path_cache_.find(module_name);
        if (cache_it != path_cache_.end()) {
            return cache_it->second;
        }

        std::string module_str = string_id::to_string(module_name);

        // Try different file extensions
        std::vector<std::string> extensions = { ".bin", ".bc", ".dx", "" };

        for (const auto& search_path : search_paths_) {
            for (const auto& ext : extensions) {
                std::filesystem::path candidate = search_path / (module_str + ext);

                if (std::filesystem::exists(candidate) &&
                    std::filesystem::is_regular_file(candidate)) {

                    path_cache_[module_name] = candidate;
                    lg::debug("Found module '{}' at: {}", module_str, candidate.string());
                    return candidate;
                }
            }
        }

        lg::error("Module '{}' not found in search paths", module_str);
        return "";
    }

} // namespace vm