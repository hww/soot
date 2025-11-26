#include "common/runtime/lib/StringId.hpp"
#include "fmt/format.h"

#include <fstream>
#include <algorithm>
#include <vector>
#include <unordered_map>

namespace runtime::lib {

    namespace {
        std::unordered_map<StringId, std::string> g_string_table;
        bool g_loaded = false;
    }

    namespace string_id {

        void load_table(const std::string& filename) {
            std::ifstream file(filename);
            if (!file) {
                throw std::runtime_error(fmt::format("Cannot load string table: {}", filename));
            }

            std::string line;
            size_t line_num = 0;

            while (std::getline(file, line)) {
                ++line_num;

                // Skip empty lines and comments
                if (line.empty() || line[0] == '#') continue;

                // Find space separator between CRC32 and string
                auto space_pos = line.find(' ');
                if (space_pos == std::string::npos) {
                    throw std::runtime_error(
                        fmt::format("Invalid format at line {} in {}: missing space",
                            line_num, filename)
                    );
                }

                // Parse CRC32 (hex without 0x prefix)
                std::string crc_str = line.substr(0, space_pos);
                StringId id;
                try {
                    id = std::stoul(crc_str, nullptr, 16);
                }
                catch (const std::exception& e) {
                    throw std::runtime_error(
                        fmt::format("Invalid CRC32 format at line {} in {}: '{}'",
                            line_num, filename, crc_str)
                    );
                }

                // Get the actual string (rest of the line after space)
                std::string str = line.substr(space_pos + 1);

                // Check for collisions (shouldn't happen in valid files)
                auto it = g_string_table.find(id);
                if (it != g_string_table.end() && it->second != str) {
                    throw std::runtime_error(
                        fmt::format("CRC32 collision in {} at line {}: '{}' and '{}' both map to {:08X}",
                            filename, line_num, it->second, str, id)
                    );
                }

                g_string_table[id] = std::move(str);
            }

            g_loaded = true;
        }

        void save_table(const std::string& filename) {
            std::ofstream file(filename);
            if (!file) {
                throw std::runtime_error(fmt::format("Cannot save string table: {}", filename));
            }

            // Sort by string value for readable output
            std::vector<std::pair<StringId, std::string>> sorted_entries(
                g_string_table.begin(), g_string_table.end()
            );

            std::sort(sorted_entries.begin(), sorted_entries.end(),
                [](const auto& a, const auto& b) {
                    return a.second < b.second;
                }
            );

            // Write entries
            for (const auto& [id, str] : sorted_entries) {
                file << fmt::format("{:08X} {}\n", id, str);
            }
        }

        std::string inspect() {
            std::string result;
            result += fmt::format("StringId table size {}\n", g_string_table.size());
            for (const auto& [id, str] : g_string_table) {
                result += fmt::format("{:08X} {}\n", id, str);
            }
            return result;
        }

        std::string to_string(StringId id) {
            auto it = g_string_table.find(id);
            if (it != g_string_table.end()) {
                return it->second;
            }
            return fmt::format("<unknown:{:08X}>", id);
        }

        const char* to_cstring(StringId id) {
            auto it = g_string_table.find(id);
            if (it != g_string_table.end()) {
                return it->second.c_str();
            }
            return "<unknown>";
        }

        bool is_table_loaded() {
            return g_loaded;
        }

        size_t get_string_count() {
            return g_string_table.size();
        }

        void clear_table() {
            g_string_table.clear();
            g_loaded = false;
        }

        // ============================================================================
        // String Registration
        // ============================================================================

        StringId register_string(const std::string& str) {
            StringId id = util::compute_crc32(str.c_str(), str.size());

            auto it = g_string_table.find(id);
            if (it != g_string_table.end()) {
                // Проверяем коллизию
                if (it->second != str) {
                    throw std::runtime_error(
                        fmt::format("CRC32 collision: '{}' and '{}' both map to {:08X}",
                            it->second, str, id)
                    );
                }
                // Такая же строка уже есть - возвращаем существующий ID
                return id;
            }

            // Новая строка - добавляем в таблицу
            g_string_table[id] = str;
            g_loaded = true;
            return id;
        }

        StringId register_string(const char* str) {
            return register_string(std::string(str));
        }
    }
} // namespace vm::string_id