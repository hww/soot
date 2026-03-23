#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/sooti/Reader.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/CommonTypes.hpp"
#include <filesystem>
#include <fstream> 

using namespace runtime::lib;

namespace runtime::files {

    struct DciFile {
        std::string logical_path;    // "math/random" - ПОЛНЫЙ логический путь
        std::string module_name;     // "random" - только имя модуля
        u32 binary_size;
        std::vector<StringId> imports;  // логические пути импортов
        std::vector<StringId> exports;  // имена экспортируемых функций

        bool is_valid() const {
            return !logical_path.empty() && !module_name.empty() && binary_size > 0;
        }

        // Извлекаем имя модуля из логического пути
        static std::string extract_module_name(const std::string& logical_path) {
            size_t last_slash = logical_path.find_last_of('/');
            if (last_slash != std::string::npos) {
                return logical_path.substr(last_slash + 1);
            }
            return logical_path; // если нет слэша, то весь путь это имя
        }

        static DciFile parse(const std::string& filename) {
            script::Reader reader;

            // Парсим без top-level обёртки
            auto obj = reader.read_from_file({ filename }, true, false);

            return parse_from_object(obj);
        }

            // common/carbon/files/DciFile.cpp - добавить:
    bool save(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file) return false;
        fmt::print("DciFile save {}\n", filename);
        // UTF-8 BOM
        file << "\xEF\xBB\xBF";
        file << to_string();
        return true;
    }

    std::string to_string() const {
        std::string result;
        result += "(" + logical_path + " (" + std::to_string(binary_size) + ")\n";
        result += "  (import";
        for (auto imp : imports) {
            result += " " + lib::to_string(imp);
        }
        result += ")\n";
        result += "  (export";
        for (auto exp : exports) {
            result += " " + lib::to_string(exp);
        }
        result += ")\n";
        result += "  (strings";
        for (auto exp : string_id::get_string_table()) {
            result += " " + exp.second;
        }
        result += ")\n";
        result += ")\n";
        return result;
    }

    private:
        static DciFile parse_from_object(const script::Object& obj) {
            DciFile result;

            // obj должен быть списком: ((math/random (324386) ...))
            if (!obj.is_pair()) {
                throw std::runtime_error("DCI file should contain a single non-empty list");
            }

            // Получаем внутренний список: (math/random (324386) ...)
            auto iterator = obj.as_pair()->car;

            if (!iterator.is_pair()) {
                throw std::runtime_error("Expected non-empty module definition list");
            }

            // 1. Logical path: math/random
            auto logical_path_obj = iterator.as_pair()->car;
            if (!logical_path_obj.is_symbol()) {
                throw std::runtime_error("Expected symbol for module logical path");
            }

            result.logical_path = logical_path_obj.as_symbol().c_str();
            result.module_name = extract_module_name(result.logical_path);

            iterator = iterator.as_pair()->cdr;

            // 2. Binary size: (324386)
            if (!iterator.is_pair()) {
                throw std::runtime_error("Expected binary size list");
            }

            auto size_list_obj = iterator.as_pair()->car;
            result.binary_size = parse_binary_size(size_list_obj);

            iterator = iterator.as_pair()->cdr;

            // 3. Process remaining elements (import/export)
            while (iterator.is_pair()) {
                auto element = iterator.as_pair()->car;
                parse_import_export(element, result);
                iterator = iterator.as_pair()->cdr;
            }

            // Проверяем правильное завершение
            if (!iterator.is_null()) {
                throw std::runtime_error("Malformed DCI file - improper list termination");
            }

            return result;
        }

        static u32 parse_binary_size(const script::Object& obj) {
            // Ожидаем: (324386) - список с одним integer
            if (!obj.is_pair()) {
                throw std::runtime_error("Expected list for binary size");
            }

            auto size_list = obj;
            auto first_element = size_list.as_pair()->car;

            // Проверяем что это число
            if (!first_element.is_integer()) {
                throw std::runtime_error("Binary size should be an integer");
            }

            // Проверяем что список содержит только один элемент
            auto rest = size_list.as_pair()->cdr;
            if (!rest.is_null()) {
                throw std::runtime_error("Binary size list should contain exactly one integer");
            }

            return static_cast<u32>(first_element.as_integer());
        }

        static void parse_import_export(const script::Object& obj, DciFile& result) {
            if (!obj.is_pair()) {
                throw std::runtime_error("Expected non-empty list for import/export");
            }

            auto list = obj;
            auto keyword_obj = list.as_pair()->car;

            if (!keyword_obj.is_symbol()) {
                throw std::runtime_error("Expected symbol as import/export keyword");
            }

            auto keyword = string_id::register_string(keyword_obj.as_symbol().c_str());
            list = list.as_pair()->cdr;

            if (keyword == string_id::register_string("import")) {
                while (list.is_pair()) {
                    auto import_name_obj = list.as_pair()->car;
                    if (!import_name_obj.is_symbol()) {
                        throw std::runtime_error("Expected symbol in import list");
                    }
                    // Импорты - это логические пути других модулей
                    result.imports.push_back(string_id::register_string(import_name_obj.as_symbol().c_str()));
                    list = list.as_pair()->cdr;
                }
            }
            else if (keyword == string_id::register_string("export")) {
                while (list.is_pair()) {
                    auto export_name_obj = list.as_pair()->car;
                    if (!export_name_obj.is_symbol()) {
                        throw std::runtime_error("Expected symbol in export list");
                    }
                    // Экспорты - это имена функций внутри модуля
                    result.exports.push_back(string_id::register_string(export_name_obj.as_symbol().c_str()));
                    list = list.as_pair()->cdr;
                }
            }
            else if (keyword == string_id::register_string("strings")) {
                while (list.is_pair()) {
                    auto export_name_obj = list.as_pair()->car;
                    if (!export_name_obj.is_symbol()) {
                        throw std::runtime_error("Expected symbol in export list");
                    }
                    // Экспорты - это имена функций внутри модуля
                    string_id::register_string(export_name_obj.as_symbol().c_str());
                    list = list.as_pair()->cdr;
                }
            }
            else {
                throw std::runtime_error("Expected 'import' or 'export' keyword, got: " +
                    std::string(keyword_obj.as_symbol().c_str()));
            }

            // Проверяем правильное завершение списка
            if (!list.is_null()) {
                throw std::runtime_error("Malformed import/export list");
            }
        }
    };

} // namespace vm