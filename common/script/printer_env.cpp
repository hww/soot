#include "printer_env.h"
#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ranges.h>
#include <sstream>
#include <algorithm>

namespace script {

    std::string EnvironmentPrettyPrinter::to_string(const EnvironmentObject& env,
        bool include_parents,
        int max_parent_depth) {
        std::stringstream ss;
        std::unordered_set<const EnvironmentObject*> visited;

        collect_environment_info(env, include_parents, max_parent_depth, visited,
            [&](const EnvironmentObject& current_env, int depth, bool is_parent) {
                std::string indent(depth * 2, ' ');

                if (is_parent) {
                    ss << fmt::format("{}Parent Environment:\n", indent);
                    indent += "  ";
                }

                // Заголовок environment
                ss << fmt::format("{}{} ({} variables)\n",
                    indent,
                    current_env.name.empty() ? "<unnamed environment>" : fmt::format("\"{}\"", current_env.name),
                    current_env.vars.m_used_entries);

                // Переменные
                bool has_vars = false;
                for (const auto& entry : current_env.vars.m_entries) {
                    if (entry.key) {
                        has_vars = true;
                        ss << fmt::format("  {}  {} = {}\n",
                            indent,
                            entry.key,
                            truncate_string(entry.value.print()));
                    }
                }

                if (!has_vars) {
                    ss << fmt::format("  {}  [no variables]\n", indent);
                }

                if (!is_parent && depth > 0) {
                    ss << "\n";
                }
            });

        return ss.str();
    }

    // Перегрузка для указателя
    std::string EnvironmentPrettyPrinter::to_string(const EnvironmentObject* env,
        bool include_parents,
        int max_parent_depth) {
        if (!env) {
            return "Null environment";
        }
        return to_string(*env, include_parents, max_parent_depth);
    }

    std::string EnvironmentPrettyPrinter::to_string(const Object& env_obj,
        bool include_parents,
        int max_parent_depth) {
        if (!env_obj.is_env()) {
            return fmt::format("Not an environment object: {}", env_obj.print());
        }
        return to_string(env_obj.as_env(), include_parents, max_parent_depth);
    }

    std::string EnvironmentPrettyPrinter::to_string(const std::shared_ptr<EnvironmentObject>& env,
        bool include_parents,
        int max_parent_depth) {
        if (!env) {
            return "Null environment";
        }
        return to_string(*env, include_parents, max_parent_depth);
    }

    std::string EnvironmentPrettyPrinter::to_tree_string(const EnvironmentObject& env,
        bool include_parents,
        int max_parent_depth) {
        std::stringstream ss;
        std::unordered_set<const EnvironmentObject*> visited;
        bool first_env = true;

        collect_environment_info(env, include_parents, max_parent_depth, visited,
            [&](const EnvironmentObject& current_env, int depth, bool is_parent) {
                if (!first_env) {
                    ss << "\n";
                }
                first_env = false;

                std::string indent(depth * 2, ' ');

                // Заголовок environment
                if (current_env.name.empty()) {
                    ss << fmt::format("{}Environment (depth: {}, vars: {})\n",
                        indent, depth, current_env.vars.m_used_entries);
                }
                else {
                    ss << fmt::format("{}\"{}\" (depth: {}, vars: {})\n",
                        indent, current_env.name, depth, current_env.vars.m_used_entries);
                }

                // Переменные
                for (const auto& entry : current_env.vars.m_entries) {
                    if (entry.key) {
                        ss << fmt::format("{}  {}: {}\n",
                            indent,
                            entry.key,
                            truncate_string(entry.value.print()));
                    }
                }
            });

        return ss.str();
    }

    std::string EnvironmentPrettyPrinter::to_tree_string(const EnvironmentObject* env,
        bool include_parents,
        int max_parent_depth) {
        if (!env) {
            return "Null environment";
        }
        return to_tree_string(*env, include_parents, max_parent_depth);
    }

    std::string EnvironmentPrettyPrinter::to_compact_string(const EnvironmentObject& env) {
        std::vector<std::string> vars;
        for (const auto& entry : env.vars.m_entries) {
            if (entry.key) {
                vars.push_back(fmt::format("{}: {}", entry.key, truncate_string(entry.value.print(), 20)));
            }
        }

        return fmt::format("Environment\"{}\" [{}]",
            env.name.empty() ? "unnamed" : env.name,
            fmt::join(vars, ", "));
    }

    std::string EnvironmentPrettyPrinter::to_compact_string(const EnvironmentObject* env) {
        if (!env) {
            return "Null environment";
        }
        return to_compact_string(*env);
    }

    std::string EnvironmentPrettyPrinter::to_colored_string(const EnvironmentObject& env,
        bool include_parents,
        int max_parent_depth) {
        std::stringstream ss;
        std::unordered_set<const EnvironmentObject*> visited;

        collect_environment_info(env, include_parents, max_parent_depth, visited,
            [&](const EnvironmentObject& current_env, int depth, bool is_parent) {
                std::string indent(depth * 2, ' ');

                if (is_parent) {
                    ss << fmt::format(fg(fmt::color::gray), "{}↳ Parent Environment:\n", indent);
                    indent += "  ";
                }

                // Заголовок environment с цветом
                auto env_name = current_env.name.empty() ?
                    "<unnamed environment>" :
                    fmt::format("\"{}\"", current_env.name);

                ss << fmt::format(fg(fmt::color::cyan), "{}", indent);
                ss << fmt::format(fg(fmt::color::light_blue), "{}", env_name);
                ss << fmt::format(fg(fmt::color::gray), " ({} variables)\n",
                    current_env.vars.m_used_entries);

                // Переменные
                for (const auto& entry : current_env.vars.m_entries) {
                    if (entry.key) {
                        ss << fmt::format("  {}  ", indent);
                        ss << fmt::format(fg(fmt::color::green), "{}", entry.key);
                        ss << fmt::format(fg(fmt::color::white), " = ");
                        ss << fmt::format(fg(fmt::color::yellow), "{}",
                            truncate_string(entry.value.print()));
                        ss << "\n";
                    }
                }

                if (!current_env.vars.m_used_entries) {
                    ss << fmt::format(fg(fmt::color::gray), "  {}  [no variables]\n", indent);
                }

                if (!is_parent && depth > 0) {
                    ss << "\n";
                }
            });

        return ss.str();
    }

    std::string EnvironmentPrettyPrinter::to_colored_string(const EnvironmentObject* env,
        bool include_parents,
        int max_parent_depth) {
        if (!env) {
            return fmt::format(fg(fmt::color::red), "Null environment");
        }
        return to_colored_string(*env, include_parents, max_parent_depth);
    }

    void EnvironmentPrettyPrinter::print(const EnvironmentObject& env,
        bool include_parents,
        int max_parent_depth) {
        fmt::print("{}\n", to_string(env, include_parents, max_parent_depth));
    }

    void EnvironmentPrettyPrinter::print(const EnvironmentObject* env,
        bool include_parents,
        int max_parent_depth) {
        if (!env) {
            fmt::print("Null environment\n");
            return;
        }
        print(*env, include_parents, max_parent_depth);
    }

    void EnvironmentPrettyPrinter::print_tree(const EnvironmentObject& env,
        bool include_parents,
        int max_parent_depth) {
        fmt::print("{}\n", to_tree_string(env, include_parents, max_parent_depth));
    }

    void EnvironmentPrettyPrinter::print_tree(const EnvironmentObject* env,
        bool include_parents,
        int max_parent_depth) {
        if (!env) {
            fmt::print("Null environment\n");
            return;
        }
        print_tree(*env, include_parents, max_parent_depth);
    }

    // Вспомогательные функции (остаются без изменений)
    void EnvironmentPrettyPrinter::collect_environment_info(
        const EnvironmentObject& env,
        bool include_parents,
        int max_parent_depth,
        std::unordered_set<const EnvironmentObject*>& visited,
        std::function<void(const EnvironmentObject&, int, bool)> callback) {

        if (visited.count(&env)) {
            callback(env, 0, false); // Отмечаем циклическую ссылку
            return;
        }
        visited.insert(&env);

        // Вызываем callback для текущего environment
        callback(env, 0, false);

        // Рекурсивно обрабатываем родителей если нужно
        if (include_parents && env.parent_env) {
            int current_depth = 0;
            auto current = env.parent_env;

            while (current && (max_parent_depth == -1 || current_depth < max_parent_depth)) {
                if (visited.count(current.get())) {
                    // Создаем временный environment для отображения циклической ссылки
                    EnvironmentObject cyclic_env;
                    cyclic_env.name = fmt::format("[cyclic reference to: {}]", current->name);
                    callback(cyclic_env, current_depth + 1, true);
                    break;
                }
                visited.insert(current.get());

                callback(*current, current_depth + 1, true);

                current = current->parent_env;
                current_depth++;
            }
        }

        visited.erase(&env);
    }

    std::string EnvironmentPrettyPrinter::format_variable(const std::string& name, const Object& value) {
        return fmt::format("{} = {}", name, value.print());
    }

    std::string EnvironmentPrettyPrinter::truncate_string(const std::string& str, size_t max_length) {
        if (str.length() <= max_length) {
            return str;
        }
        return str.substr(0, max_length - 3) + "...";
    }

} // namespace script