#pragma once

#include "types.hpp"
#include "variant.hpp"
#include "instructions.hpp"
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

namespace vm {

    // Forward declarations
    struct BinaryFile;
    struct BinFileHeader;
    struct ByteCode;
    class Module;

    constexpr u32 align_size(u32 n) {
        return (n + 3) & ~3;
    }

    /** Single record in the data block */
    struct Record {
        union {
            u64 as_u64;
            s64 as_s64;
            s32 as_s32;
            u32 as_u32;
            f32 as_f32;
            char as_char;
            void* as_ptr;
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

    /** The virtual machine definition - УПРОЩАЕМ! */
    struct Definition {
        StringId name;
        StringId type;
        Ptr<void> data_ptr;  

        std::string to_string() const {
            return std::format("Definition('{}', '{}', ptr:{:x})",
                name, type, data_ptr.offset);
        }
    };

    constexpr u32 DC_MAGIC = ('D' | 'X' << 8 | '0' << 16 | '0' << 24);

    /** The header of the binary file - УПРОЩАЕМ! */
    struct BinFileHeader {
        u32 magic_num;
        u32 file_size;
        Ptr<Definition> defs_ptr;  // ← Готовый указатель на таблицу определений!
        u32 defs_count;            // ← Только фактическое количество!

        BinFileHeader()
            : magic_num(DC_MAGIC), file_size(0), defs_ptr(0), defs_count(0) {
        }

        bool is_valid_magic() const { return magic_num == DC_MAGIC; }

        /** Get definition by index - ПРОСТО И ЯСНО! */
        Definition* get_definition(size_t idx) const {
            if (idx >= defs_count) {
                throw ByteCodeError("Definition index out of bounds");
            }
            return (defs_ptr + idx).c();  // ← Простая арифметика Ptr!
        }

        /** Get definition pointer - ПРОСТО И ЯСНО! */
        template <typename T>
        Ptr<T> get_definition_ptr(size_t idx) const {
            auto def = get_definition(idx);
            return def->data_ptr.cast<T>();
        }

        std::string to_string() const {
            return std::format("BinFileHeader<size:{} defs:{}>", file_size, defs_count);
        }
    };

    /** The virtual machine byte code - УПРОЩАЕМ! */
    struct ByteCode {
        u32 desc_size;
        Ptr<Instruction> code_ptr;    // ← Готовый указатель на код!
        Ptr<Record> data_ptr;         // ← Готовый указатель на данные!
        Ptr<SourceLocation> debug_ptr; // ← Готовый указатель на debug info!
        u32 debug_count;

        // Методы становятся ТРИВИАЛЬНЫМИ!
        Instruction* get_code_ptr() const { return code_ptr.c(); }
        Record* get_data_ptr() const { return data_ptr.c(); }
        SourceLocation* get_debug_info() const {
            return debug_count > 0 ? debug_ptr.c() : nullptr;
        }

        SourceLocation find_source_location(u32 instruction_ip) const {
            auto debug_info = get_debug_info();
            if (!debug_info) return SourceLocation{ 0, 0, 0 };

            for (u32 i = 0; i < debug_count; ++i) {
                if (debug_info[i].instruction_offset == instruction_ip) {
                    return debug_info[i];
                }
            }
            return SourceLocation{ 0, 0, 0 };
        }

        bool has_debug_info() const { return debug_count > 0 && debug_ptr != nullptr; }
    };

    /** Main binary file class - УПРОЩАЕМ! */
    struct BinaryFile {
    private:
        std::unique_ptr<u8[]> file_data;
        BinFileHeader* header;
        Module* owner_module; 
    public:
        BinaryFile() : header(nullptr) {}

        /** Initialize new file - БЕЗ max_definitions! */
        void initialize(size_t data_size) {
            assert(data_size < std::numeric_limits<u32>::max());

            // Просто выделяем память, без сложных вычислений
            const size_t file_size = sizeof(BinFileHeader) + data_size;
            file_data = std::make_unique<u8[]>(file_size);
            header = reinterpret_cast<BinFileHeader*>(file_data.get());

            // Инициализируем простой заголовок
            new (header) BinFileHeader();
            header->file_size = static_cast<u32>(file_size);
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

        /** Construct file from pool **/
        static std::unique_ptr<BinaryFile> create_from_pool(void* pool_data, size_t size) {
            auto file = std::make_unique<BinaryFile>();
            file->header = reinterpret_cast<BinFileHeader*>(pool_data);
            // валидация
            return file;
        }

        /** Safe access to file header */
        BinFileHeader* get_header() const {
            if (!header || !is_loaded()) {
                throw ByteCodeError("File not initialized or has been moved");
            }
            return header;
        }

        /** Get definition by index */
        Definition* get_definition(u32 idx) const {
            return get_header()->get_definition(idx);
        }

        /** Get definition pointer */
        template<typename T>
        T* get_definition_ptr(u32 idx) const {
            return get_header()->get_definition_ptr<T>(idx).c();
        }

        /** Utility methods */
        u32 get_definition_count() const {
            return is_loaded() ? header->defs_count : 0;
        }

        bool is_loaded() const {
            return header != nullptr && file_data != nullptr;
        }

        std::string to_string() const {
            if (!is_loaded()) return "#BinaryFile <uninitialized or moved>";
            return std::format("#BinaryFile <{}>", header->to_string());
        }

        // Move semantics
        BinaryFile(BinaryFile&& other) noexcept
            : file_data(std::move(other.file_data)), header(other.header) {
            other.header = nullptr;
        }

        BinaryFile& operator=(BinaryFile&& other) noexcept {
            if (this != &other) {
                file_data = std::move(other.file_data);
                header = other.header;
                other.header = nullptr;
            }
            return *this;
        }

        /** Load from file - БЫЛО УТЕРЯНО! */
        bool load_from_file(const std::string& filename) {
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                lg::error("Cannot open file: {}", filename);
                return false;
            }

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<u8> buffer(size);
            if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                lg::error("Cannot read file: {}", filename);
                return false;
            }

            load(std::move(buffer));
            return true;
        }
        
        // Новый метод: найти байткод по имени определения
        ByteCode* find_bytecode_by_name(StringId name) const {
            if (!is_loaded()) return nullptr;

            auto header = get_header();
            for (u32 i = 0; i < header->defs_count; i++) {
                auto def = header->get_definition(i);
                if (def->name == name) {
                    return header->get_definition_ptr<ByteCode>(i).c();
                }
            }
            return nullptr;
        }

        // Строковая версия для удобства
        ByteCode* find_bytecode_by_name(const std::string& name) const {
            return find_bytecode_by_name(string_id::register_string(name));
        }

        std::string inspect() const {
            if (!is_loaded()) return "#BinaryFile<unloaded>";
            return std::format("#BinaryFile<defs:{}>", header->defs_count);
        }

        // Delete copy operations
        BinaryFile(const BinaryFile&) = delete;
        BinaryFile& operator=(const BinaryFile&) = delete;
    };

    /** Binary file builder - УПРОЩАЕМ! */
    class BinaryFileBuilder {
    private:
        std::vector<Instruction> code_segment;
        std::vector<Record> data_segment;
        std::vector<Definition> definitions;
        std::vector<SourceLocation> debug_info;

    public:
        BinaryFileBuilder() = default;  // ← БЕЗ max_definitions!

        /** Add a definition */
        void add_definition(StringId name, StringId type) {
            definitions.emplace_back(Definition{ name, type, Ptr<void>(0) });
        }

        /** Add code instructions */
        void add_code(const std::vector<Instruction>& instructions) {
            code_segment.insert(code_segment.end(), instructions.begin(), instructions.end());
        }

        /** Add function */
        void add_function(StringId name, const std::vector<Instruction>& instructions) {
            add_definition(name, SID("function"));
            add_code(instructions);
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
            // Calculate sizes
            const u32 code_size = align_size(static_cast<u32>(code_segment.size() * sizeof(Instruction)));
            const u32 data_size = align_size(static_cast<u32>(data_segment.size() * sizeof(Record)));
            const u32 debug_size = align_size(static_cast<u32>(debug_info.size() * sizeof(SourceLocation)));
            const u32 defs_size = static_cast<u32>(definitions.size() * sizeof(Definition));

            const u32 total_size = sizeof(BinFileHeader) + defs_size +
                sizeof(ByteCode) + code_size + data_size + debug_size;

            // Create buffer
            std::vector<u8> buffer(total_size);
            BinFileHeader* header = reinterpret_cast<BinFileHeader*>(buffer.data());

            // Initialize simple header
            new (header) BinFileHeader();
            header->file_size = total_size;
            header->defs_count = static_cast<u32>(definitions.size());

            // Setup all sections
            setup_sections(header, buffer, defs_size, code_size, data_size, debug_size);

            return buffer;
        }

        std::unique_ptr<BinaryFile> build_file() {
            std::vector<u8> data = build();
            auto file = std::make_unique<BinaryFile>();
            file->load(std::move(data));
            return file;
        }
        std::string inspect() const {
            return std::format("#BinaryFileBuilder<defs:{}, code:{}, data:{}>",
                definitions.size(), code_segment.size(), data_segment.size());
        }
    private:
        void setup_sections(BinFileHeader* header, std::vector<u8>& buffer,
            u32 defs_size, u32 code_size, u32 data_size, u32 debug_size) {

            // Calculate absolute offsets in file
            u32 current_offset = sizeof(BinFileHeader);

            // 1. Definitions table
            header->defs_ptr = Ptr<Definition>(current_offset);
            setup_definitions(header, buffer, current_offset);
            current_offset += defs_size;

            // 2. ByteCode structure  
            u32 bytecode_offset = current_offset;
            ByteCode* bytecode = reinterpret_cast<ByteCode*>(buffer.data() + bytecode_offset);
            current_offset += sizeof(ByteCode);

            // 3. Code section
            bytecode->code_ptr = Ptr<Instruction>(current_offset);
            setup_code(bytecode, buffer, current_offset);
            current_offset += code_size;

            // 4. Data section
            bytecode->data_ptr = Ptr<Record>(current_offset);
            setup_data(bytecode, buffer, current_offset);
            current_offset += data_size;

            // 5. Debug section
            if (debug_size > 0) {
                bytecode->debug_ptr = Ptr<SourceLocation>(current_offset);
                bytecode->debug_count = static_cast<u32>(debug_info.size());
                setup_debug(bytecode, buffer, current_offset);
            }
            else {
                bytecode->debug_ptr = Ptr<SourceLocation>(0);
                bytecode->debug_count = 0;
            }

            // Finalize ByteCode
            bytecode->desc_size = sizeof(ByteCode) + code_size + data_size + debug_size;

            // Update definitions to point to bytecode
            update_definition_pointers(header, bytecode_offset);
        }

        void setup_definitions(BinFileHeader* header, std::vector<u8>& buffer, u32 offset) {
            if (definitions.empty()) return;

            Definition* defs_table = reinterpret_cast<Definition*>(buffer.data() + offset);
            std::memcpy(defs_table, definitions.data(), definitions.size() * sizeof(Definition));
        }

        void setup_code(ByteCode* bytecode, std::vector<u8>& buffer, u32 offset) {
            if (code_segment.empty()) return;

            Instruction* code_table = reinterpret_cast<Instruction*>(buffer.data() + offset);
            std::memcpy(code_table, code_segment.data(), code_segment.size() * sizeof(Instruction));
        }

        void setup_data(ByteCode* bytecode, std::vector<u8>& buffer, u32 offset) {
            if (data_segment.empty()) return;

            Record* data_table = reinterpret_cast<Record*>(buffer.data() + offset);
            std::memcpy(data_table, data_segment.data(), data_segment.size() * sizeof(Record));
        }

        void setup_debug(ByteCode* bytecode, std::vector<u8>& buffer, u32 offset) {
            if (debug_info.empty()) return;

            SourceLocation* debug_table = reinterpret_cast<SourceLocation*>(buffer.data() + offset);
            std::memcpy(debug_table, debug_info.data(), debug_info.size() * sizeof(SourceLocation));
        }

        void update_definition_pointers(BinFileHeader* header, u32 bytecode_offset) {
            for (u32 i = 0; i < header->defs_count; i++) {
                Definition* def = header->get_definition(i);
                def->data_ptr = Ptr<void>(bytecode_offset);  // Все определения указывают на ByteCode
            }
        }
    };

} // namespace vm