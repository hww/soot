#pragma once

#include "object.h"
#include <fmt/format.h>
#include <string>
#include <unordered_set>
#include <functional>
#include <memory>

namespace script {

    class EnvironmentPrettyPrinter {
    public:
        // Основные функции вывода
        static std::string to_string(const EnvironmentObject& env,
            bool include_parents = false,
            int max_parent_depth = -1);

        // Перегрузки для указателей
        static std::string to_string(const EnvironmentObject* env,
            bool include_parents = false,
            int max_parent_depth = -1);

        static std::string to_string(const Object& env_obj,
            bool include_parents = false,
            int max_parent_depth = -1);

        static std::string to_string(const std::shared_ptr<EnvironmentObject>& env,
            bool include_parents = false,
            int max_parent_depth = -1);

        // Древовидный формат (как в Lua)
        static std::string to_tree_string(const EnvironmentObject& env,
            bool include_parents = false,
            int max_parent_depth = -1);

        static std::string to_tree_string(const EnvironmentObject* env,
            bool include_parents = false,
            int max_parent_depth = -1);

        // Компактный формат (одна строка)
        static std::string to_compact_string(const EnvironmentObject& env);
        static std::string to_compact_string(const EnvironmentObject* env);

        // Форматированный вывод с цветами (если поддерживается)
        static std::string to_colored_string(const EnvironmentObject& env,
            bool include_parents = false,
            int max_parent_depth = -1);

        static std::string to_colored_string(const EnvironmentObject* env,
            bool include_parents = false,
            int max_parent_depth = -1);

        // Вывод непосредственно в консоль
        static void print(const EnvironmentObject& env,
            bool include_parents = false,
            int max_parent_depth = -1);

        static void print(const EnvironmentObject* env,
            bool include_parents = false,
            int max_parent_depth = -1);

        static void print_tree(const EnvironmentObject& env,
            bool include_parents = false,
            int max_parent_depth = -1);

        static void print_tree(const EnvironmentObject* env,
            bool include_parents = false,
            int max_parent_depth = -1);

    private:
        // Вспомогательные функции
        static void collect_environment_info(const EnvironmentObject& env,
            bool include_parents,
            int max_parent_depth,
            std::unordered_set<const EnvironmentObject*>& visited,
            std::function<void(const EnvironmentObject&, int, bool)> callback);

        static std::string format_variable(const std::string& name, const Object& value);
        static std::string truncate_string(const std::string& str, size_t max_length = 50);
    };

} // namespace script