#include "common/carbon/files/BinaryFile.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/CommonTypes.hpp"
#include "fmt/base.h"
#include <sstream>


namespace runtime::files {

    // =============================================================================
    // ByteCodeError Implementation
    // =============================================================================

    ByteCodeError::ByteCodeError(const std::string& msg) : message(msg) {}

    const char* ByteCodeError::what() const noexcept {
        return message.c_str();
    }

    // =============================================================================
    // SourceLocation Implementation  
    // =============================================================================

    std::string SourceLocation::to_string() const {
        return std::format("SourceLocation(ip:{:04x}, line:{}, file:{})",
            offset, line, file);
    }

    // =============================================================================
    // Definition Implementation
    // =============================================================================

    inline bool has_flag(SymbolFlags flags, SymbolFlags flag) {
        return (static_cast<int>(flags) & static_cast<int>(flag)) != 0;
    }

    std::string get_symbol_flags_string(SymbolFlags flags) {
        if (flags == SymbolFlags::None) return "none";
        
        std::string result;
        if (has_flag(flags, SymbolFlags::Local)) result += "local";
        if (has_flag(flags, SymbolFlags::Import)) {
            if (!result.empty()) result += "|";
            result += "import";
        }
        if (has_flag(flags, SymbolFlags::Export)) {
            if (!result.empty()) result += "|";
            result += "export";
        }
        return result;
    }

    std::string Definition::to_string() const {
        return std::format("Definition(:name '{}', :type '{}', :ptr {:x} :flags {})",
            string_id::to_cstring(name), string_id::to_cstring(type), (u64)data_ptr.offset, get_symbol_flags_string(flags));
    }

    std::string Definition::inspect() const {
        return std::format("(definition {} :type {} :ptr {:x} :flags {})",
            string_id::to_cstring(name), string_id::to_cstring(type), (u64)data_ptr.offset, get_symbol_flags_string(flags));
    }

    // =============================================================================
    // ByteCode Implementation
    // =============================================================================

    ByteCode::ByteCode() :
        code_count(0),
        data_size(0),
        debug_count(0),
        code_ptr(),
        data_ptr(),
        debug_ptr(),
        owner_module(nullptr)
    {
    }

    Instruction* ByteCode::get_code_ptr() const {
        return code_ptr.offset != 0 ? code_ptr.c() : nullptr;
    }

    u8* ByteCode::get_data_ptr() const {
        return data_ptr.offset != 0 ? data_ptr.c() : nullptr;
    }

    SourceLocation* ByteCode::get_debug_info() const {
        return debug_ptr.offset != 0 ? debug_ptr.c() : nullptr;
    }

    SourceLocation ByteCode::find_source_location(u32 instruction_ip) const {
        auto debug_info = get_debug_info();
        if (!debug_info) {
            return SourceLocation{ 0, 0, 0 };
        }

        // Linear search through debug info to find matching instruction offset
        // In production this could be optimized with binary search if entries are sorted
        for (u32 i = 0; i < debug_count; ++i) {
            if (debug_info[i].offset == instruction_ip) {
                return debug_info[i];
            }
        }

        // No debug info found for this instruction
        return SourceLocation{ 0, 0, 0 };
    }

    bool ByteCode::has_debug_info() const {
        return debug_ptr.offset != 0;
    }

    void ByteCode::relocate_pointers(intptr_t delta) {
        // Adjust all pointer offsets by the delta value
        // This is used when the bytecode is moved in memory
        if (code_ptr.offset != 0) {
            code_ptr.offset += delta;
        }
        if (data_ptr.offset != 0) {
            data_ptr.offset += delta;
        }
        if (debug_ptr.offset != 0) {
            debug_ptr.offset += delta;
        }
    }

    std::string ByteCode::inspect() const {
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

    // =============================================================================
    // BinaryFile Implementation
    // =============================================================================

    BinaryFile::BinaryFile() {
        magic = MAGIC;
        generation = CURRENT_GENERATION;
        file_size = 0;
        used_size = 0;
        definitions.offset = 0;
        definitions_count = 0;
        base_offset = 0;
        reserved = 0;
    }

    bool BinaryFile::is_valid() const {
        // Basic validation checks:
        // 1. Magic number must match
        // 2. File size must be at least header size
        // 3. Used size cannot exceed file size
        // 4. Definitions count must be reasonable (arbitrary limit of 1M)
        return magic == MAGIC &&
            base_offset != nullptr &&
            file_size >= HEADER_SIZE &&
            used_size <= file_size &&
            definitions_count < (1 << 20); // Sanity check
    }

    Definition* BinaryFile::get_definition(u32 idx) const {
        if (idx >= definitions_count) {
            throw std::runtime_error(
                std::format("Definition index {} out of bounds (count: {})",
                    idx, definitions_count));
        }

        // Calculate pointer to the definition at the given index
        Ptr<Definition> result_ptr = definitions + idx;
        Definition* result = result_ptr.c();
        return result;
    }

    Definition* BinaryFile::find_definition_by_name(StringId name) const {
        // Linear search through definitions table
        // In production this could be optimized with a hash table if needed
        for (u32 i = 0; i < definitions_count; i++) {
            auto def = get_definition(i);
            if (def->name == name) {
                return def;
            }
        }
        return nullptr;
    }

    ByteCode* BinaryFile::find_bytecode_by_name(StringId name) const {
        for (u32 i = 0; i < definitions_count; i++) {
            auto def = get_definition(i);
            if (def->name == name) {
                // Only return bytecode for function definitions
                if (def->type == type::function) {
                    return def->data_ptr.cast<ByteCode>().c();
                }
                else {
                    return nullptr;
                }
            }
        }
        return nullptr;
    }
    
    // =============================================================================
    // Relocation
    // =============================================================================

    // Вместо старого relocate_pointers
    void BinaryFile::relocate_pointers(bool to_memory) {
        u8* base = reinterpret_cast<u8*>(this);
        
        ptrdiff_t delta;
        if (to_memory) {
            delta = reinterpret_cast<ptrdiff_t>(base) - reinterpret_cast<ptrdiff_t>(base_offset);
        } else {
            delta = -reinterpret_cast<ptrdiff_t>(base_offset);
        }
        apply_delta_to_pointers(delta);
        
        if (to_memory) {
            base_offset = this;
        } else {
            base_offset = 0;
        }
    }

    void BinaryFile::apply_delta_to_pointers(ptrdiff_t delta) {
        // Применяем к definitions
        fmt::print("apply_delta_to_pointers :base {} :delta {} :newbase {}\n", 
            (void*)definitions.ptr, 
            delta, 
            (void*)((uint8_t*)definitions.ptr + delta)
        );
        definitions.ptr = reinterpret_cast<Definition*>(reinterpret_cast<u8*>(definitions.ptr) + delta);
        
        // Применяем ко всем определениям
        for (u32 i = 0; i < definitions_count; i++) {
            Definition* def = get_definition(i);
            def->data_ptr.ptr = reinterpret_cast<u8*>(def->data_ptr.ptr) + delta;
            
            if (def->type == type::function) {
                ByteCode* bc = reinterpret_cast<ByteCode*>(def->data_ptr.ptr);
                bc->code_ptr.ptr = reinterpret_cast<Instruction*>(reinterpret_cast<u8*>(bc->code_ptr.ptr) + delta);
                bc->data_ptr.ptr = reinterpret_cast<u8*>(bc->data_ptr.ptr) + delta;
                bc->debug_ptr.ptr = reinterpret_cast<SourceLocation*>(reinterpret_cast<u8*>(bc->debug_ptr.ptr) + delta);
            }
        }
    }

    // common/carbon/files/BinaryFile.cpp
    void BinaryFile::set_owner(Module* owner) {
        owner_module = owner;
        relocate_pointers(true);
        
        for (u32 i = 0; i < definitions_count; i++) {
            auto def = get_definition(i);
            if (def->type == SID("function")) {
                ByteCode* bytecode = def->data_ptr.cast<ByteCode>().c();
                if (bytecode) {
                    bytecode->owner_module = owner_module;
                }
            }
        }
    }

    // =============================================================================
    // Inspecting
    // =============================================================================

    std::string BinaryFile::to_string() const {
        return std::format("BinaryFile<gen:{}, size:{}/{}, defs:{}>",
            generation, used_size, file_size, definitions_count);
    }

    std::string BinaryFile::hex_dump() const {
        const u8* header_bytes = reinterpret_cast<const u8*>(this);
        std::string result;

        // Format header as hexadecimal bytes in groups of 4
        for (u32 i = 0; i < HEADER_SIZE; i++) {
            result += std::format("{:02x}", header_bytes[i]);
            if ((i + 1) % 4 == 0) result += " ";
        }
        return result;
    }

    std::string BinaryFile::inspect() const {
        std::stringstream result;

        result << std::format("BinaryFile[gen:{}, size:{}/{}]\n",
            generation, used_size, file_size);
        result << std::format("  Definitions: {} entries\n", definitions_count);
        result << std::format("  Magic: 0x{:08x} ({})\n", magic,
            magic == MAGIC ? "valid" : "INVALID");

        // Inspect each definition
        for (u32 i = 0; i < definitions_count; i++) {
            auto def = get_definition(i);
            result << std::format("    [{}] {}\n", i, def->inspect());

            // Add detailed information for function definitions
            if (def->type == type::function) {
                ByteCode* bc = def->data_ptr.cast<ByteCode>().c();
                if (bc) {
                    result << std::format("         -> {}\n", bc->inspect());

                    // Show code section details if available
                    if (bc->code_count > 0) {
                        result << std::format("            Code: {} instructions at 0x{:x}\n",
                            bc->code_count, bc->code_ptr.offset);
                    }

                    // Show data section details if available  
                    if (bc->data_size > 0) {
                        result << std::format("            Data: {} bytes at 0x{:x}\n",
                            bc->data_size, bc->data_ptr.offset);
                    }

                    // Show debug information if available
                    if (bc->has_debug_info()) {
                        result << std::format("            Debug: {} entries at 0x{:x}\n",
                            bc->debug_count, bc->debug_ptr.offset);
                    }
                }
            }
        }

        return result.str();
    }

} // namespace runtime::files