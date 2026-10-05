#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/StringIdManager.hpp"
#include "common/soot/Reader.hpp"
#include "util/FileUtil.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace carbon {

    /// @brief Parsed representation of a .dci module descriptor.
    /// @details A .dci file is a S-expression that lists the module's logical path,
    ///          binary size, imports, and exports:
    ///            ((math/random (324386)
    ///              (import ...)
    ///              (export ...)))
    ///          The binary_size field is informational; the actual bytecode lives
    ///          in the sibling .dc file.
    struct DCIFile {
        std::string           logical_path; ///< full logical path, e.g. "math/random"
        std::string           module_name;  ///< last path segment, e.g. "random"
        u32                   binary_size;  ///< reported size of the sibling .dc file
        std::vector<StringId> imports;      ///< logical paths this module depends on
        std::vector<StringId> exports;      ///< function names this module exports

        /// @return true if the descriptor has the mandatory fields set.
        bool is_valid() const {
            return !logical_path.empty() && !module_name.empty() && binary_size > 0;
        }

        /// @brief Extract the module name from a logical path (last '/' segment).
        static std::string extract_module_name(const std::string &logical_path) {
            const size_t last_slash = logical_path.find_last_of('/');
            return last_slash != std::string::npos ? logical_path.substr(last_slash + 1)
                                                   : logical_path;
        }

        /// @brief Parse a .dci file from disk.
        static DCIFile parse(const std::string &filename) {
            soot::Reader reader;
            auto         obj = reader.read_from_file({filename}, true, false);
            return parse_from_object(obj);
        }

        /// @brief Serialise this descriptor and write it to disk (UTF-8 with BOM).
        /// @return true on success.
        bool save(const std::string &filename) const {
            file_util::create_dirs_for_file(filename);
            std::ofstream file(filename);
            if (!file) { return false; }
            fmt::print("DciFile save {}\n", filename);
            file << "\xEF\xBB\xBF"; // UTF-8 BOM
            file << to_string();
            return true;
        }

        /// @brief Serialise this descriptor to an S-expression string.
        std::string to_string() const {
            std::string result;
            result += "(" + logical_path + " (" + std::to_string(binary_size) + ")\n";

            result += "  (import";
            for (const auto &imp : imports) { result += " " + imp.to_string(); }
            result += ")\n";

            result += "  (export";
            for (const auto &exp : exports) { result += " " + exp.to_string(); }
            result += ")\n";

            result += "  (strings";
            for (const auto &[id, str] : StringIdManager::instance()) { result += " " + str; }
            result += ")\n";

            result += ")\n";
            return result;
        }

    private:
        /// @brief Convert a parsed soot::Object tree into a DCIFile.
        static DCIFile parse_from_object(const soot::Object &obj);
        static u32     parse_binary_size(const soot::Object &obj);
        static void    parse_import_export(const soot::Object &obj, DCIFile &result);
    };

} // namespace carbon