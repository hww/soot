#pragma once
#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "ptr.hpp"
#include "instructions.hpp"
#include "util/assert.h"
#include "util/log.h"
#include <vector>
#include <format>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace vm {

    // Forward declarations
    struct BinaryFile;
    struct BinFileHeader;
    struct ByteCode;

    /**
     * Align the size to 4-byte boundary
     */
    constexpr u32 align_size(u32 n) {
        return (n + 3) & ~3;
    }

    /** Single record in the data block */
    struct Record {
        union {
            u64 as_uint64;
            u32 as_int32[2];
            f32 as_float[2];
            char as_char[8];
        };
    };

    /** Source location for debug information */
    struct SourceLocation {
        u32 instruction_offset;
        u32 source_line;
        StringId source_file;

        std::string to_string() const {
            return std::format("SourceLocation(ip:{:04x}, line:{}, file:{})",
                instruction_offset, source_line, source_file);
        }
    };

    class ByteCodeError : public std::exception {
    public:
        explicit ByteCodeError(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

    /** The virtual machine definition */
    struct Definition {
        StringId name;
        StringId type;
        u32 offset;

        void define(StringId name, StringId type, u32 offset) {
            this->name = name;
            this->type = type;
            this->offset = offset;
        }

        std::string to_string() const {
            return std::format("Definition('{}', '{}', offset:{:x})", name, type, offset);
        }
    };

    constexpr u32 DC_MAGIC = ('D' | 'X' << 8 | '0' << 16 | '0' << 24);

    /** The header of the binary file */
    struct BinFileHeader {
        // Data members first
        u32 magic_num;
        u32 file_size;
        u32 used_size;
        u32 defs_max;
        u32 defs_num;
        u32 defs_offs;
        u32 offset;

        BinFileHeader(StringId id)
            : magic_num(DC_MAGIC), file_size(0), used_size(sizeof(BinFileHeader)),
            offset(0), defs_max(0), defs_num(0), defs_offs(0) {
        }

        /** Verify if the header is valid */
        bool is_valid_magic() const { return magic_num == DC_MAGIC; }

        /** Get free size of the file */
        size_t get_free_size() const {
            return (used_size > file_size) ? 0 : file_size - used_size;
        }

        /** Initialize the definitions table */
        void init_definitions_table(u32 max_definitions, u32 file_size) {
            size_t required_size = sizeof(BinFileHeader) + sizeof(Definition) * max_definitions;
            if (required_size > file_size) {
                throw ByteCodeError("Not enough space for definitions table");
            }

            magic_num = DC_MAGIC;
            defs_num = 0;
            offset = 0;
            defs_max = max_definitions;
            this->file_size = file_size;
            defs_offs = sizeof(BinFileHeader);
            used_size = defs_offs + sizeof(Definition) * max_definitions;
        }

        /** Get definition by index */
        Ptr<Definition> get_definition(size_t idx) const {
            if (idx >= defs_max) {
                throw ByteCodeError("Definition index out of bounds");
            }
            Ptr<const void> base = make_ptr(this);
            return Ptr<Definition>(base.offset + defs_offs + idx * sizeof(Definition));
        }

        /** Define new object - non-templated version */
        Ptr<void> define(StringId name, StringId type) {
            if (defs_num >= defs_max) {
                throw ByteCodeError("Definitions table overflow");
            }
            auto definition = get_definition(defs_num);
            definition->define(name, type, used_size);
            defs_num++;

            Ptr<void> result = make_ptr(this).cast<void>() + used_size;
            return result;
        }

        /** Define new object - templated version */
        template<class T>
        Ptr<T> define(StringId name, StringId type) {
            Ptr<void> result = define(name, type);
            used_size += sizeof(T); // Reserve space for the object
            return result.cast<T>();
        }

        /** Get definition pointer - non-templated version */
        Ptr<void> get_definition_ptr(size_t idx) const {
            auto def = get_definition(idx);
            Ptr<const void> base = make_ptr(this);
            return base.cast<void>() + def->offset;
        }

        /** Get definition pointer - templated version */
        template <typename T>
        Ptr<T> get_definition_ptr(size_t idx) const {
            return get_definition_ptr(idx).cast<T>();
        }

        std::string to_string() const {
            return std::format("BinFileHeader<size:{}/{} defs:{}/{}>",
                used_size, file_size, defs_num, defs_max);
        }
    };

    /** The virtual machine byte code with integrated debug info */
    struct ByteCode {
        u32 desc_size;
        u32 file_offset;
        u32 code_offset;
        u32 data_offset;
        u32 debug_count;
        u32 debug_offset;

        // Method implementations after BinFileHeader is complete
        Ptr<Instruction> get_code_ptr(Ptr<BinFileHeader> file_ptr) const;
        Ptr<Record> get_data_ptr(Ptr<BinFileHeader> file_ptr) const;
        Ptr<SourceLocation> get_debug_info(Ptr<BinFileHeader> file_ptr) const;
        SourceLocation find_source_location(Ptr<BinFileHeader> file_ptr, u32 instruction_ip) const;
        bool has_debug_info() const { return debug_count > 0 && debug_offset != 0; }
    };

    // Implement ByteCode methods after BinFileHeader definition
    inline Ptr<Instruction> ByteCode::get_code_ptr(Ptr<BinFileHeader> file_ptr) const {
        if (code_offset >= file_ptr->file_size) {
            throw ByteCodeError("Code section out of bounds");
        }
        return Ptr<Instruction>(file_ptr.offset + code_offset);
    }

    inline Ptr<Record> ByteCode::get_data_ptr(Ptr<BinFileHeader> file_ptr) const {
        if (data_offset >= file_ptr->file_size) {
            throw ByteCodeError("Data section out of bounds");
        }
        return Ptr<Record>(file_ptr.offset + data_offset);
    }

    inline Ptr<SourceLocation> ByteCode::get_debug_info(Ptr<BinFileHeader> file_ptr) const {
        if (debug_count == 0 || debug_offset == 0) return Ptr<SourceLocation>();
        if (debug_offset >= file_ptr->file_size) {
            throw ByteCodeError("Debug section out of bounds");
        }
        return Ptr<SourceLocation>(file_ptr.offset + debug_offset);
    }

    inline SourceLocation ByteCode::find_source_location(Ptr<BinFileHeader> file_ptr, u32 instruction_ip) const {
        auto debug_info = get_debug_info(file_ptr);
        if (!debug_info.valid()) return SourceLocation{ 0, 0, 0 };

        for (u32 i = 0; i < debug_count; ++i) {
            if (debug_info[i].instruction_offset == instruction_ip) {
                return debug_info[i];
            }
        }
        return SourceLocation{ 0, 0, 0 };
    }

    /** Binary file builder for safe construction */
    class BinaryFileBuilder {
    private:
        std::vector<Instruction> code_segment;
        std::vector<Record> data_segment;
        std::vector<Definition> definitions;
        std::vector<SourceLocation> debug_info;
        u32 max_definitions;

    public:
        explicit BinaryFileBuilder(u32 max_defs = 1000)
            : max_definitions(max_defs) {
            definitions.reserve(max_definitions);
        }

        /** Add a definition with bounds checking */
        void add_definition(StringId name, StringId type) {
            if (definitions.size() >= max_definitions) {
                throw ByteCodeError("Definitions table overflow");
            }

            u32 offset = calculate_next_offset();
            definitions.emplace_back(Definition{ name, type, offset });
        }

        /** Add code instructions */
        void add_code(const std::vector<Instruction>& instructions) {
            code_segment.insert(code_segment.end(), instructions.begin(), instructions.end());
        }

        /** Add data records */
        void add_data(const std::vector<Record>& records) {
            data_segment.insert(data_segment.end(), records.begin(), records.end());
        }

        /** Add debug information */
        void add_debug_info(u32 instruction_offset, u32 source_line, StringId source_file) {
            debug_info.push_back(SourceLocation{ instruction_offset, source_line, source_file });
        }

        /** Build final binary file */
        std::vector<u8> build() {
            // Calculate total size with alignment
            const u32 code_size = align_size(code_segment.size() * sizeof(Instruction));
            const u32 data_size = align_size(data_segment.size() * sizeof(Record));
            const u32 debug_size = align_size(debug_info.size() * sizeof(SourceLocation));
            const u32 defs_size = definitions.size() * sizeof(Definition);

            const u32 total_size = sizeof(BinFileHeader) + defs_size +
                sizeof(ByteCode) + code_size + data_size + debug_size;

            // Create buffer
            std::vector<u8> buffer(total_size);
            Ptr<BinFileHeader> header = make_ptr(reinterpret_cast<BinFileHeader*>(buffer.data()));

            // Initialize header
            header->init_definitions_table(static_cast<u32>(definitions.size()), total_size);

            // Copy definitions
            copy_definitions(header, buffer);

            // Create and initialize bytecode
            setup_bytecode(header, buffer, code_size, data_size, debug_size);

            return buffer;
        }

    private:
        u32 calculate_next_offset() const {
            u32 offset = sizeof(BinFileHeader) + definitions.size() * sizeof(Definition);
            offset += sizeof(ByteCode); // Reserve space for ByteCode header
            return offset;
        }

        void copy_definitions(Ptr<BinFileHeader> header, std::vector<u8>& buffer) {
            if (definitions.empty()) return;

            u8* defs_start = buffer.data() + header->defs_offs;
            std::memcpy(defs_start, definitions.data(), definitions.size() * sizeof(Definition));
        }

        void setup_bytecode(Ptr<BinFileHeader> header, std::vector<u8>& buffer,
            u32 code_size, u32 data_size, u32 debug_size) {
            // Calculate offsets
            u32 bytecode_offset = sizeof(BinFileHeader) + definitions.size() * sizeof(Definition);
            u32 code_offset = bytecode_offset + sizeof(ByteCode);
            u32 data_offset = code_offset + code_size;
            u32 debug_offset = debug_size > 0 ? data_offset + data_size : 0;

            // Get ByteCode pointer
            Ptr<ByteCode> bytecode = make_ptr(reinterpret_cast<ByteCode*>(
                buffer.data() + bytecode_offset));

            // Initialize ByteCode header
            bytecode->desc_size = sizeof(ByteCode) + code_size + data_size + debug_size;
            bytecode->file_offset = bytecode_offset;
            bytecode->code_offset = code_offset;
            bytecode->data_offset = data_offset;
            bytecode->debug_count = static_cast<u32>(debug_info.size());
            bytecode->debug_offset = debug_offset;

            // Copy code
            if (!code_segment.empty()) {
                u8* code_start = buffer.data() + code_offset;
                std::memcpy(code_start, code_segment.data(),
                    code_segment.size() * sizeof(Instruction));
            }

            // Copy data
            if (!data_segment.empty()) {
                u8* data_start = buffer.data() + data_offset;
                std::memcpy(data_start, data_segment.data(),
                    data_segment.size() * sizeof(Record));
            }

            // Copy debug info
            if (!debug_info.empty()) {
                u8* debug_start = buffer.data() + debug_offset;
                std::memcpy(debug_start, debug_info.data(),
                    debug_info.size() * sizeof(SourceLocation));
            }
        }
    };

    /** Main binary file class with safe memory management */
    struct BinaryFile {
    private:
        std::unique_ptr<u8[]> file_data;
        BinFileHeader* header;

    public:
        BinaryFile() : header(nullptr) {}

        /** Initialize new file */
        void initialize(u32 max_definitions, size_t data_size) {
            assert(data_size < std::numeric_limits<u32>::max());

            const size_t file_size = sizeof(BinFileHeader) +
                sizeof(Definition) * max_definitions +
                data_size;

            file_data = std::make_unique<u8[]>(file_size);
            header = reinterpret_cast<BinFileHeader*>(file_data.get());
            header->init_definitions_table(max_definitions, static_cast<u32>(file_size));
        }

        /** Load from existing data */
        void load(std::vector<u8>&& data) {
            file_data = std::make_unique<u8[]>(data.size());
            std::memcpy(file_data.get(), data.data(), data.size());
            header = reinterpret_cast<BinFileHeader*>(file_data.get());

            if (!header->is_valid_magic()) {
                throw ByteCodeError("Invalid file magic");
            }
        }

        /** Safe access to file header */
        Ptr<BinFileHeader> get_header() const {
            if (!header) throw ByteCodeError("File not initialized");
            return make_ptr(header);
        }

        /** Define new object */
        template<class T>
        Ptr<T> define(StringId name, StringId type) {
            return header->define<T>(name, type);
        }

        Ptr<void> define(StringId name, StringId type) {
            return header->define(name, type);
        }

        /** Get definition by index */
        Ptr<Definition> get_definition(u32 idx) const {
            return header->get_definition(idx);
        }

        /** Get definition pointer */
        template<typename T>
        Ptr<T> get_definition_ptr(u32 idx) const {
            return header->get_definition_ptr<T>(idx);
        }

        /** Utility methods */
        size_t get_used_size() const { return header ? header->used_size : 0; }
        size_t get_free_size() const { return header ? header->get_free_size() : 0; }
        u32 get_definition_count() const { return header ? header->defs_num : 0; }
        bool is_loaded() const { return header != nullptr; }

        std::string to_string() const {
            if (!header) return "#BinaryFile <uninitialized>";
            return std::format("#BinaryFile <{}>", header->to_string());
        }

        // Rule of five
        BinaryFile(BinaryFile&&) = default;
        BinaryFile& operator=(BinaryFile&&) = default;
        BinaryFile(const BinaryFile&) = delete;
        BinaryFile& operator=(const BinaryFile&) = delete;
    };

} // namespace vm