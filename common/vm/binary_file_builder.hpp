#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "binary_file_pool.hpp"
#include "ptr.hpp"  
#include "binary_file.hpp"
#include "module.hpp"
#include "util/assert.h"
#include "util/log.h"
#include <vector>
#include <format>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <cassert>
#include <functional>

namespace vm {

    /** Binary file builder - создает модули с BinaryFile */
 /** Binary file builder - простой последовательный билдер */
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
        std::shared_ptr<Module> build_and_load_to_pool(StringId module_name);

        std::string inspect_input() const;
        std::string inspect_memory_dump(const std::vector<u8>& binary) const;
        std::string inspect_build_result(const std::vector<u8>& binary) const;
        void debug_print_input() const;
        void debug_dump_binary(const std::vector<u8>& binary) const;
        void debug_full_inspect(const std::vector<u8>& binary) const;

    private:
        void ensure_capacity(std::vector<u8>& buffer, u32 required_size) {
            if (required_size > buffer.size()) {
                // Увеличиваем буфер с запасом (удваиваем или +50%)
                u32 new_size = std::max(required_size, buffer.size() * 2);
                buffer.resize(new_size);
            }
        }

        void setup_bytecode_owners(BinaryFile* binary_file, Module* owner_module) {
            for (u32 i = 0; i < binary_file->definitions_count; i++) {
                auto def = binary_file->get_definition(i);
                if (def->type == SID("function")) {
                    ByteCode* bytecode = def->data_ptr.cast<ByteCode>().c();
                    if (bytecode) {
                        bytecode->owner_module = owner_module;
                    }
                }
            }
        }

        void build_export_table(const std::shared_ptr<Module>& module) {
            if (!module->is_binary_loaded()) return;

            BinaryFile* file = module->binary_file;
            for (u32 i = 0; i < file->definitions_count; i++) {
                auto def = file->get_definition(i);
                module->add_export(def->name, def);
            }
        }

        static constexpr u32 align_size(u32 n) {
            return (n + 3) & ~3;
        }
    };

} // namespace vm