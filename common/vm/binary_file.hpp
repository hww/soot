#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "binary_file_pool.hpp"
#include "ptr.hpp"  
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
#include <vector>
#include <format>
#include <stdexcept>

namespace vm {

    // Forward declarations
    struct BinaryFile;
    struct BinFileHeader;
    struct ByteCode;
    class Module;
    class ModulePool;

    constexpr u32 align_size(u32 n) {
        return (n + 3) & ~3;
    }


    /* Erros */
    class ByteCodeError : public std::exception {
    public:
        explicit ByteCodeError(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

    /** Source location for debug information */
    struct SourceLocation {
        /** Character offset in file */
        u32 offset;
        /** The line of expression */
        u32 line;
        /** The file path */
        StringId file;

        std::string to_string() const {
            return std::format("SourceLocation(ip:{:04x}, line:{}, file:{})",
                offset, line, file);
        }
    };


    /** The virtual machine definition - УПРОЩАЕМ! */
    struct Definition {
        /** The definition's name */
        StringId name;
        /** The definition's type */
        StringId type;
        /** The definition's offset from begin of file */
        Ptr<void> data_ptr;  

        std::string to_string() const {
            return std::format("Definition('{}', '{}', ptr:{:x})",
                string_id::to_cstring(name), string_id::to_cstring(type), data_ptr.offset);
        }

        std::string inspect() const {
            return std::format("(definition {} ({}) :ptr {:x})",
                string_id::to_cstring(name), string_id::to_cstring(type), data_ptr.offset);
        }
    };

    struct Descriptor {
        // The size of the data and code
        u32 desc_size;
        virtual void relocate_pointers(intptr_t delta) = 0;
    };

    /** The virtual machine byte code */
    struct ByteCode : public Descriptor {
        /** Количество инструкций или 0 если нет */
        u32 code_count;
        /** Размер данных в байтах */
        u32 data_size;
        /** Размер отладочных данных */
        u32 debug_count;
        /** Положение кода (нет кода если offset == 0) */
        Ptr<Instruction> code_ptr;     
        /** Положение констант (нет констант если offset == 0) */
        Ptr<u8> data_ptr;          
        /** Отладочные данные (нет данных если или offset == 0) */
        Ptr<SourceLocation> debug_ptr; 
        /** Ссылка на владельца (в файле равна 0) настраивается по требованию резолвера */
        Module* owner_module;
        
        ByteCode() = default;

        Instruction* get_code_ptr() const {
            return code_ptr.offset != 0 ? code_ptr.c() : nullptr;
        }

        u8* get_data_ptr() const {
            return data_ptr.offset != 0 ? data_ptr.c() : nullptr;
        }
        
        SourceLocation* get_debug_info() const {
            return debug_ptr.offset != 0 ? debug_ptr.c() : nullptr;
        }

        SourceLocation find_source_location(u32 instruction_ip) const {
            auto debug_info = get_debug_info();
            if (!debug_info) return SourceLocation{ 0, 0, 0 };

            for (u32 i = 0; i < code_count; ++i) {
                if (debug_info[i].offset == instruction_ip) {
                    return debug_info[i];
                }
            }
            return SourceLocation{ 0, 0, 0 };
        }

        bool has_debug_info() const {
            return debug_ptr.offset != 0;
        }

        virtual void relocate_pointers(intptr_t delta) {
            code_ptr.offset += delta;
            data_ptr.offset += delta;
            debug_ptr.offset += delta;
        }

        std::string inspect() const {
            std::string code_info = code_ptr.offset != 0 ?
                std::format("{} instructions", code_count) : "no code";
            std::string data_info = data_ptr.offset != 0 ?
                std::format("{} bytes", data_size) : "no data";
            std::string debug_info = debug_ptr.offset != 0 ?
                std::format("{} entries", debug_count) : "no debug";

            return std::format("ByteCode(code: {}, data: {}, debug: {}, owner: {})",
                code_info, data_info, debug_info,
                owner_module ? "set" : "null");
        }

    private:
    };


    /** Binary File - только заголовок и определения */
    struct BinaryFile {
        // === Фиксированный заголовок (32 байта) ===
        u32 magic;                      // 4 байта для валидации
        u32 generation;                 // 4 байта для версии
        u32 file_size;                  // 4 байта размер файла
        u32 used_size;                  // 4 байта 

        // Таблица определений
        Ptr<Definition> definitions;    // 4 байта
        u32 definitions_count;          // 4 байта

        // Резерв (для выравнивания до 32 байт)
        u32 base_offset;                // 4 байта
        u32 reserved;                   // 4 байта
                                        // Итого: 32 байта
      
        // Константы
        static constexpr u32 MAGIC = ('D' | 'X' << 8 | '0' << 16 | '0' << 24);
        static constexpr u32 HEADER_SIZE = 32;
        static constexpr u32 CURRENT_GENERATION = 1;

        BinaryFile() {
            magic = MAGIC;
            generation = CURRENT_GENERATION;
            file_size = 0;
            used_size = 0;
            definitions.offset = 0;
            definitions_count = 0;
            base_offset = 0;
            reserved = 0;
        }

        bool is_valid() const {
            return magic == MAGIC && file_size >= HEADER_SIZE;
        }

        Definition* get_definition(u32 idx) const {
            if (idx >= definitions_count) {
                throw std::runtime_error("Definition index out of bounds");
            }

            lg::info("get_definition: g_module_pool_base = {}, definitions.offset = {}",
                (void*)g_module_pool_base, definitions.offset);

            Ptr<Definition> result_ptr = definitions + idx;
            Definition* result = result_ptr.c();

            lg::info("result_ptr.offset = {}, result = {}", result_ptr.offset, (void*)result);

            return result;
        }

        /** Найти ByteCode по имени определения */
        ByteCode* find_bytecode_by_name(StringId name) const {
            for (u32 i = 0; i < definitions_count; i++) {
                auto def = get_definition(i);
                if (def->name == name) {
                    return def->data_ptr.cast<ByteCode>().c();
                }
            }
            return nullptr;
        }

        /** Реиндексация указателей */
        void relocate_pointers(void* pool_base);

        std::string to_string() const {
            return std::format("BinaryFile<gen:{}, size:{}/{}, defs:{}>",
                generation, used_size, file_size, definitions_count);
        }

        std::string hex_dump() const {
            const u8* header_bytes = reinterpret_cast<const u8*>(this);
            std::string result;
            for (u32 i = 0; i < HEADER_SIZE; i++) {
                result += std::format("{:02x}", header_bytes[i]);
                if ((i + 1) % 4 == 0) result += " ";
            }
            return result;
        }

        std::string inspect() const {
            std::string result = std::format("BinaryFile[gen:{}, size:{}/{}]\n",
                generation, used_size, file_size);
            result += std::format("  Definitions: {} entries\n", definitions_count);

            for (u32 i = 0; i < definitions_count; i++) {
                auto def = get_definition(i);
                result += std::format("    [{}] {}\n", i, def->inspect());

                // Если это функция, покажем дополнительную информацию о байткоде
                if (def->type == type::function) {
                    ByteCode* bc = def->data_ptr.cast<ByteCode>().c();
                    if (bc) {
                        result += std::format("         -> {}\n", bc->inspect());
                    }
                }
            }
            return result;
        }

    };

} // namespace vm