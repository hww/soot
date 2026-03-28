#include "common/CommonTypes.hpp"
#include "common/carbon/files/BinaryFile.hpp"
#include "common/carbon/modules/Module.hpp"
#include "common/carbon/files/Definition.hpp"
#include "common/carbon/files/FunctionDesc.hpp"
#include "files/StateDesc.hpp"
#include "files/TypeDesc.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "lib/Variant.hpp"
#include <cstddef>
#include <sstream>

using namespace carbon::lib;

namespace carbon::files {


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

        return &definitions.ptr[idx];
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

    FunctionDesc* BinaryFile::find_function_by_name(StringId name) const {
        for (u32 i = 0; i < definitions_count; i++) {
            auto def = get_definition(i);
            if (def->name == name) {
                // Only return FunctionDesc for function definitions
                if (def->type == TypeIds::function) {
                    return def->data.cast<FunctionDesc>().get();
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
    void BinaryFile::relocate_pointers(bool to_memory, Module* owner) {
        u8* base = reinterpret_cast<u8*>(this);
        
        ptrdiff_t delta;
        if (to_memory) {
            delta = reinterpret_cast<ptrdiff_t>(base) - reinterpret_cast<ptrdiff_t>(base_offset);
        } else {
            delta = -reinterpret_cast<ptrdiff_t>(base_offset);
        }

        if (delta != 0)
            apply_delta_to_pointers(to_memory, delta, owner);
        
        if (to_memory) {
            base_offset = this;
        } else {
            base_offset = 0;
        }
    }

    void BinaryFile::apply_delta_to_pointers(bool to_memory, ptrdiff_t delta, Module* module) {
        owner_module = module;
        // Применяем к definitions
        fmt::print("apply_delta_to_pointers :base {} :delta {} :newbase {}\n", 
            (void*)definitions.ptr, 
            delta, 
            (void*)((uint8_t*)definitions.ptr + delta)
        );
        if (to_memory) {
            definitions.offset += delta;
            Definition::relocate_pointers_table(to_memory, delta, definitions.ptr, definitions_count, module);
        } else {
            Definition::relocate_pointers_table(to_memory, delta, definitions.ptr, definitions_count, module);
            definitions.offset += delta;
        }
    }

    // =============================================================================
    // Inspecting
    // =============================================================================

    std::string BinaryFile::to_string() const {
        return std::format("BinaryFile<gen:{}, size:{}/{}, defs:{}>",
            generation, used_size, file_size, definitions_count);
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
            result << std::format("Definition[{}] {}\n", i, def->inspect());

            // Add detailed information for function definitions
            if (def->type == TypeIds::function) {
                FunctionDesc* bc = def->data.cast<FunctionDesc>().get();
                result << fmt::format("  {}\n", bc->inspect());
            }
            else if (def->type == TypeIds::type) {
                TypeDesc* bc = def->data.cast<TypeDesc>().get();
                result << fmt::format("  {}\n", bc->inspect());
            }
            else if (def->type == TypeIds::state) {
                StateDesc* bc = def->data.cast<StateDesc>().get();
                result << fmt::format("  {}\n", bc->inspect());
            }            
        }

        return result.str();
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

    std::string BinaryFile::dump() const {
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
        }

        return result.str();
    }

} // namespace carbon::files