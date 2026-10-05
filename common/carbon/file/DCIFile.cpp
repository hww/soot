#include "DCIFile.hpp"

namespace carbon {

    DCIFile DCIFile::parse_from_object(const soot::Object &obj) {
        DCIFile result;

        // Expected top-level: a single non-empty list.
        if (!obj.is_pair()) {
            throw std::runtime_error("DCI file should contain a single non-empty list");
        }

        // Inner list: (math/random (324386) ...)
        auto iterator = obj.as_pair()->car;
        if (!iterator.is_pair()) {
            throw std::runtime_error("Expected non-empty module definition list");
        }

        // 1. Logical path.
        const auto logical_path_obj = iterator.as_pair()->car;
        if (!logical_path_obj.is_symbol()) {
            throw std::runtime_error("Expected symbol for module logical path");
        }
        result.logical_path = logical_path_obj.as_symbol().c_str();
        result.module_name = extract_module_name(result.logical_path);

        iterator = iterator.as_pair()->cdr;

        // 2. Binary size: (324386).
        if (!iterator.is_pair()) { throw std::runtime_error("Expected binary size list"); }
        result.binary_size = parse_binary_size(iterator.as_pair()->car);

        iterator = iterator.as_pair()->cdr;

        // 3. Remaining elements: import / export / strings.
        while (iterator.is_pair()) {
            parse_import_export(iterator.as_pair()->car, result);
            iterator = iterator.as_pair()->cdr;
        }

        if (!iterator.is_null()) {
            throw std::runtime_error("Malformed DCI file - improper list termination");
        }

        return result;
    }

    u32 DCIFile::parse_binary_size(const soot::Object &obj) {
        if (!obj.is_pair()) { throw std::runtime_error("Expected list for binary size"); }

        const auto first = obj.as_pair()->car;
        if (!first.is_integer()) { throw std::runtime_error("Binary size should be an integer"); }

        const auto rest = obj.as_pair()->cdr;
        if (!rest.is_null()) {
            throw std::runtime_error("Binary size list should contain exactly one integer");
        }

        return static_cast<u32>(first.as_integer());
    }

    void DCIFile::parse_import_export(const soot::Object &obj, DCIFile &result) {
        if (!obj.is_pair()) {
            throw std::runtime_error("Expected non-empty list for import/export");
        }

        auto       list = obj;
        const auto keyword_obj = list.as_pair()->car;
        if (!keyword_obj.is_symbol()) {
            throw std::runtime_error("Expected symbol as import/export keyword");
        }

        const StringId keyword(keyword_obj.as_symbol().c_str());
        list = list.as_pair()->cdr;

        if (keyword == StringId("import")) {
            while (list.is_pair()) {
                const auto name_obj = list.as_pair()->car;
                if (!name_obj.is_symbol()) {
                    throw std::runtime_error("Expected symbol in import list");
                }
                result.imports.emplace_back(name_obj.as_symbol().c_str());
                list = list.as_pair()->cdr;
            }
        } else if (keyword == StringId("export")) {
            while (list.is_pair()) {
                const auto name_obj = list.as_pair()->car;
                if (!name_obj.is_symbol()) {
                    throw std::runtime_error("Expected symbol in export list");
                }
                result.exports.emplace_back(name_obj.as_symbol().c_str());
                list = list.as_pair()->cdr;
            }
        } else if (keyword == StringId("strings")) {
            while (list.is_pair()) {
                const auto name_obj = list.as_pair()->car;
                if (!name_obj.is_symbol()) {
                    throw std::runtime_error("Expected symbol in strings list");
                }
                // No storage target for strings currently; parsed for validation only.
                list = list.as_pair()->cdr;
            }
        } else {
            throw std::runtime_error(
                std::string("Expected 'import', 'export' or 'strings' keyword, got: ") +
                keyword_obj.as_symbol().c_str());
        }

        if (!list.is_null()) { throw std::runtime_error("Malformed import/export list"); }
    }

} // namespace carbon