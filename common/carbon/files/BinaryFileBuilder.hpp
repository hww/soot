#pragma once

#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/vm/Instructions.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/carbon/files/TypeDesc.hpp"
#include "common/carbon/files/RelocatableBuffer.hpp"
#include <vector>
#include <algorithm>
#include <memory>
#include <cassert>
#include <string>

using namespace carbon::lib;
using namespace carbon::modules;

namespace carbon::files {

    /** Binary file builder - создает модули с BinaryFile */
    class BinaryFileBuilder {
    private:
        std::string name;

        struct DefinitionData {
            std::string name;
            std::string type;
            SymbolFlags flags;
            RelocatableBuffer data;  // Для простых данных
        };

        std::vector<DefinitionData> definitions_;

    public:
        BinaryFileBuilder(std::string name) { 
            this->name = name;
        }

        /** Добавить простую дефиницию */
        void add_definition(std::string name, std::string type, const RelocatableBuffer data, SymbolFlags flags = SymbolFlags::Export) {
            definitions_.push_back(DefinitionData{ name, type, flags, std::move(data) });
        }

                
        /** Построить бинарник - ПРОСТОЙ ВАРИАНТ */
        std::vector<u8> build();
        std::shared_ptr<Module> build_module();
        BinaryFile* build_file();

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