#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "common/carbon/files/BinaryFile.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "common/carbon/files/BinaryFile.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/util/Assert.hpp"
#include "common/util/Log.hpp"
#include <vector>
#include <format>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <cassert>
#include <functional>
#include <string>

using namespace runtime::lib;
using namespace runtime::modules;

namespace runtime::files {

    /** Binary file builder - создает модули с BinaryFile */
 /** Binary file builder - простой последовательный конструктор */
    class BinaryFileBuilder {
    private:
        struct DefinitionData {
            StringId name;
            StringId type;
            std::vector<u8> data;  // Для простых данных
            std::vector<Instruction> code;  // Для функций
            std::vector<SourceLocation> debug_info;  // Для функций
        };

        std::vector<DefinitionData> definitions_;

    public:
        BinaryFileBuilder() = default;

        /** Добавить функцию */
        void add_function(StringId name, const std::vector<Instruction>& code,
            const std::vector<u8>& data = {},
            const std::vector<SourceLocation>& debug_info = {}) {
            definitions_.push_back(DefinitionData{ name, SID("function"), data, code, debug_info });
        }

        /** Добавить простую дефиницию */
        void add_definition(StringId name, StringId type, const std::vector<u8>& data) {
            definitions_.push_back(DefinitionData{ name, type, data, {}, {} });
        }

        /** Построить бинарник - ПРОСТОЙ ВАРИАНТ */
        std::vector<u8> build();
        std::shared_ptr<Module> build_module(StringId module_name);

        std::string inspect_input() const;
        std::string inspect_memory_dump(const std::vector<u8>& binary) const;
        std::string inspect_build_result(const std::vector<u8>& binary) const;
        void debug_print_input() const;
        void debug_dump_binary(const std::vector<u8>& binary) const;
        void debug_full_inspect(const std::vector<u8>& binary) const;

    private:
        void ensure_capacity(std::vector<u8>& buffer, size_t required_size) {
            if (required_size > buffer.size()) {
                // Увеличиваем буфер с запасом (удваиваем или +50%)
                u32 new_size = std::max(required_size, buffer.size() * 2);
                buffer.resize(new_size);
            }
        }


        void build_export_table(const std::shared_ptr<Module>& module);

        static constexpr u32 align_size(u32 n) {
            return (n + 3) & ~3;
        }
    };

} // namespace vm