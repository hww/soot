#pragma once

#include <optional>
#include <string>

// Метаданные для определений (типов, методов, состояний)
struct DefinitionMetadata {
    // Информация о местоположении в исходном коде
    struct ShortInfo {
        struct SourceLocation {
            std::string filename;
            int line = 0;
            int column = 0;

            std::string to_string() const {
                return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
            }

            // ДОБАВИТЬ операторы сравнения
            bool operator==(const SourceLocation& other) const {
                return filename == other.filename && line == other.line && column == other.column;
            }

            bool operator!=(const SourceLocation& other) const {
                return !(*this == other);
            }
        };

        SourceLocation location;
        std::string form_content;  // Исходный текст формы

        std::string to_string() const {
            return location.to_string() + " -> " + form_content;
        }

        // ДОБАВИТЬ операторы сравнения для ShortInfo
        bool operator==(const ShortInfo& other) const {
            return location == other.location && form_content == other.form_content;
        }

        bool operator!=(const ShortInfo& other) const {
            return !(*this == other);
        }
    };

    // Основные поля
    std::optional<ShortInfo> definition_info;  // Где определено
    std::optional<std::string> docstring;      // Документация

    // Методы для удобства
    bool has_location() const { return definition_info.has_value(); }
    bool has_docstring() const { return docstring.has_value(); }

    std::string get_location_string() const {
        if (definition_info) {
            return definition_info->location.to_string();
        }
        return "unknown";
    }

    std::string get_docstring_or_empty() const {
        return docstring.value_or("");
    }

    // Операторы для тестирования
    bool operator==(const DefinitionMetadata& other) const {
        return definition_info == other.definition_info && docstring == other.docstring;
    }

    bool operator!=(const DefinitionMetadata& other) const {
        return !(*this == other);
    }
};