#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
#include "util/assert.h"
#include "util/log.h"
#include <vector>
#include <format>
#include <algorithm>

namespace vm {

    // ============================================================================
    // Binary Format Constants
    // ============================================================================

    constexpr u32 DC_MAGIC = 'D' | ('C' << 8) | ('0' << 16) | ('0' << 24); // "DC00"
    constexpr u32 CURRENT_VERSION = 1;

    // ============================================================================
    // Binary File Structures
    // ============================================================================

#pragma pack(push, 1)

    // ----------------------------------------------------------------------------
    // Definition Entry
    // ----------------------------------------------------------------------------
    struct Definition {
        StringId name;      // Name of the definition
        StringId type;      // Type ("lambda", "s32", "float", etc.)
        u32 offset;         // Offset from start of file

        void initialize(StringId def_name, StringId def_type, u32 def_offset) {
            name = def_name;
            type = def_type;
            offset = def_offset;
        }

        std::string to_string() const {
            return std::format("Definition(name:{}, type:{}, offset:{:x})",
                string_id_to_string(name), string_id_to_string(type), offset);
        }
    };

    // ----------------------------------------------------------------------------
    // ByteCode Structure
    // ----------------------------------------------------------------------------
    struct ByteCode {
        u32 desc_size;      // Total size of this descriptor (header + code + data)
        u32 file_offset;    // Offset from start of file
        u32 code_offset;    // Offset to code section
        u32 data_offset;    // Offset to data section

        // ------------------------------------------------------------------------
        // Pointer Accessors
        // ------------------------------------------------------------------------
        Instruction* get_code_ptr() {
            return reinterpret_cast<Instruction*>(
                reinterpret_cast<std::uintptr_t>(this) + (code_offset - file_offset));
        }

        const Instruction* get_code_ptr() const {
            return reinterpret_cast<const Instruction*>(
                reinterpret_cast<std::uintptr_t>(this) + (code_offset - file_offset));
        }

        Variant* get_data_ptr() {
            return reinterpret_cast<Variant*>(
                reinterpret_cast<std::uintptr_t>(this) + (data_offset - file_offset));
        }

        const Variant* get_data_ptr() const {
            return reinterpret_cast<const Variant*>(
                reinterpret_cast<std::uintptr_t>(this) + (data_offset - file_offset));
        }

        // ------------------------------------------------------------------------
        // Size Accessors
        // ------------------------------------------------------------------------
        u32 get_code_size() const {
            return (data_offset - code_offset) / sizeof(Instruction);
        }

        u32 get_data_size() const {
            return (desc_size - (data_offset - file_offset)) / sizeof(Variant);
        }

        u32 get_total_size() const {
            return desc_size;
        }

        // ------------------------------------------------------------------------
        // Initialization
        // ------------------------------------------------------------------------
        void initialize(std::intptr_t file_base, const std::vector<Instruction>& code,
            const std::vector<Variant>& data) {
            // Calculate offsets
            file_offset = static_cast<u32>(reinterpret_cast<std::intptr_t>(this) - file_base);
            desc_size = sizeof(ByteCode) +
                static_cast<u32>(code.size() * sizeof(Instruction)) +
                static_cast<u32>(data.size() * sizeof(Variant));
            code_offset = file_offset + sizeof(ByteCode);
            data_offset = code_offset + static_cast<u32>(code.size() * sizeof(Instruction));

            // Copy code and data
            if (!code.empty()) {
                Instruction* code_ptr = get_code_ptr();
                std::copy(code.begin(), code.end(), code_ptr);
            }

            if (!data.empty()) {
                Variant* data_ptr = get_data_ptr();
                std::copy(data.begin(), data.end(), data_ptr);
            }
        }

        std::string to_string() const {
            return std::format("ByteCode(size:{}, file_offset:{:x}, code_offset:{:x}, data_offset:{:x}, code_size:{}, data_size:{})",
                desc_size, file_offset, code_offset, data_offset, get_code_size(), get_data_size());
        }
    };

    // ----------------------------------------------------------------------------
    // File Header
    // ----------------------------------------------------------------------------
    struct FileHeader {
        u32 magic;          // DC_MAGIC
        u32 version;        // CURRENT_VERSION
        u32 file_size;      // Total file size
        u32 used_size;      // Used bytes in file
        u32 defs_count;     // Number of definitions
        u32 defs_offset;    // Offset to definitions table
        u32 reserved;       // Reserved for future use

        // ------------------------------------------------------------------------
        // Validation
        // ------------------------------------------------------------------------
        bool is_valid() const {
            return magic == DC_MAGIC && version <= CURRENT_VERSION;
        }

        void initialize(u32 total_size, u32 max_definitions) {
            magic = DC_MAGIC;
            version = CURRENT_VERSION;
            file_size = total_size;
            defs_count = 0;
            defs_offset = sizeof(FileHeader);
            used_size = defs_offset + sizeof(Definition) * max_definitions;
            reserved = 0;
        }

        // ------------------------------------------------------------------------
        // Definition Access
        // ------------------------------------------------------------------------
        Definition* get_definition(u32 index) {
            ASSERT_FORMAT(index < defs_count, "Definition index out of bounds: {} (count: {})",
                index, defs_count);
            Definition* def_table = reinterpret_cast<Definition*>(
                reinterpret_cast<std::uintptr_t>(this) + defs_offset);
            return &def_table[index];
        }

        const Definition* get_definition(u32 index) const {
            ASSERT_FORMAT(index < defs_count, "Definition index out of bounds: {}", index);
            return reinterpret_cast<const Definition*>(
                reinterpret_cast<std::uintptr_t>(this) + defs_offset) + index;
        }

        std::uintptr_t get_definition_ptr(u32 index) const {
            const Definition* def = get_definition(index);
            return reinterpret_cast<std::uintptr_t>(this) + def->offset;
        }

        template<typename T>
        T* get_definition_as(u32 index) {
            return reinterpret_cast<T*>(get_definition_ptr(index));
        }

        // ------------------------------------------------------------------------
        // Definition Management
        // ------------------------------------------------------------------------
        Definition* add_definition(StringId name, StringId type, u32 data_size) {
            // Проверяем есть ли место для нового определения
            u32 new_defs_count = defs_count + 1;
            ASSERT_MSG(new_defs_count * sizeof(Definition) <= (file_size - defs_offset),
                "Definition table overflow");

            // Вычисляем указатель на новое определение напрямую, без get_definition
            Definition* def_table = reinterpret_cast<Definition*>(
                reinterpret_cast<std::uintptr_t>(this) + defs_offset);
            Definition* new_def = &def_table[defs_count];  // Прямой доступ к массиву

            new_def->initialize(name, type, used_size);

            defs_count = new_defs_count;  // Обновляем счетчик
            used_size += data_size;

            return new_def;
        }


        std::string to_string() const {
            return std::format("FileHeader(magic:{:08x}, version:{}, size:{}/{}, defs:{})",
                magic, version, used_size, file_size, defs_count);
        }
    };

#pragma pack(pop)

    static_assert(sizeof(Definition) == 12, "Definition size mismatch");
    static_assert(sizeof(ByteCode) == 16, "ByteCode size mismatch");
    static_assert(sizeof(FileHeader) == 4*7, "FileHeader size mismatch");

    // ============================================================================
    // Binary File Manager
    // ============================================================================

    class BinaryFile {
    public:
        BinaryFile() : header_(nullptr), data_(nullptr) {}

        ~BinaryFile() {
            cleanup();
        }

        // ------------------------------------------------------------------------
        // Creation
        // ------------------------------------------------------------------------
        void create(u32 max_definitions, u32 total_size) {
            cleanup();

            total_size = std::max(total_size, sizeof(FileHeader) + sizeof(Definition) * max_definitions);
            data_ = new u8[total_size];
            header_ = reinterpret_cast<FileHeader*>(data_);

            header_->initialize(total_size, max_definitions);
        }

        // ------------------------------------------------------------------------
        // Definition Management
        // ------------------------------------------------------------------------
        ByteCode* add_function(StringId name, const std::vector<Instruction>& code,
            const std::vector<Variant>& data = {}) {
            ASSERT_MSG(header_ != nullptr, "Binary file not initialized");

            // Вычисляем размер
            u32 required_size = sizeof(ByteCode) +
                static_cast<u32>(code.size() * sizeof(Instruction)) +
                static_cast<u32>(data.size() * sizeof(Variant));

            // Добавляем определение
            Definition* def = header_->add_definition(name, "lambda"_sid, required_size);

            // Создаем байткод
            ByteCode* bytecode = reinterpret_cast<ByteCode*>(
                reinterpret_cast<std::uintptr_t>(header_) + def->offset);
            bytecode->initialize(reinterpret_cast<std::uintptr_t>(header_), code, data);

            return bytecode;
        }


        Definition* add_data(StringId name, StringId type, const Variant& value) {
            ASSERT_MSG(header_ != nullptr, "Binary file not initialized");

            std::vector<Variant> data = { value };
            u32 required_size = sizeof(ByteCode) +
                static_cast<u32>(data.size() * sizeof(Variant));

            // Добавляем определение и возвращаем его
            Definition* def = header_->add_definition(name, type, required_size);

            // Создаем байткод объект
            ByteCode* bytecode = reinterpret_cast<ByteCode*>(
                reinterpret_cast<std::uintptr_t>(header_) + def->offset);
            bytecode->initialize(reinterpret_cast<std::uintptr_t>(header_), {}, data);

            return def;
        }


        // ------------------------------------------------------------------------
        // Accessors
        // ------------------------------------------------------------------------
        FileHeader* get_header() { return header_; }
        const FileHeader* get_header() const { return header_; }

        ByteCode* get_function(StringId name) {
            if (!header_) return nullptr;

            for (u32 i = 0; i < header_->defs_count; i++) {
                const Definition* def = header_->get_definition(i);
                if (def->name == name && def->type == "lambda"_sid) {
                    return header_->get_definition_as<ByteCode>(i);
                }
            }
            return nullptr;
        }

        u32 get_definition_count() const {
            return header_ ? header_->defs_count : 0;
        }

        const Definition* get_definition(u32 index) const {
            return header_ ? header_->get_definition(index) : nullptr;
        }

        // ------------------------------------------------------------------------
        // File I/O (basic interface)
        // ------------------------------------------------------------------------
        bool save_to_file(const std::string& filename) const {
            // Implementation would use file_util
            lg::info("Saving binary file: {} ({} bytes)", filename,
                header_ ? header_->used_size : 0);
            return true; // Placeholder
        }

        bool load_from_file(const std::string& filename) {
            // Implementation would use file_util
            lg::info("Loading binary file: {}", filename);
            return true; // Placeholder
        }

        // ------------------------------------------------------------------------
        // Debugging
        // ------------------------------------------------------------------------
        std::string to_string() const {
            if (!header_) return "BinaryFile<null>";
            return std::format("BinaryFile({})", header_->to_string());
        }

        void dump_definitions() const {
            if (!header_) return;

            lg::info("=== Binary File Definitions ===");
            for (u32 i = 0; i < header_->defs_count; i++) {
                const Definition* def = header_->get_definition(i);
                lg::info("  [{}] {}", i, def->to_string());
            }
        }

    private:
        void cleanup() {
            if (data_) {
                delete[] data_;
                data_ = nullptr;
                header_ = nullptr;
            }
        }

        FileHeader* header_;
        u8* data_;
    };

    // ============================================================================
    // Utility Functions
    // ============================================================================

    inline std::ostream& operator<<(std::ostream& os, const BinaryFile& file) {
        return os << file.to_string();
    }

    inline std::ostream& operator<<(std::ostream& os, const Definition& def) {
        return os << def.to_string();
    }

    inline std::ostream& operator<<(std::ostream& os, const ByteCode& code) {
        return os << code.to_string();
    }

} // namespace vm