#pragma once
#include "common/carbon/ForwardDeclarations.hpp"
#include "common/CommonTypes.hpp"
#include "common/carbon/lib/Variant.hpp"
#include "common/carbon/lib/Ptr.hpp"  
#include "common/carbon/vm/Instructions.hpp"
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

using namespace runtime::lib;
using namespace runtime::vm;
using namespace runtime::modules;

namespace runtime::files {

    /**
     * @brief Aligns size to 4-byte boundary
     * @param n Original size to align
     * @return Size aligned to 4 bytes
     *
     * This is critical for proper memory alignment in the bytecode format
     * as many architectures require 4-byte aligned memory access for
     * optimal performance and correctness.
     */
    constexpr u32 align_size(u32 n) {
        return (n + 3) & ~3;
    }

    /* Errors */
    /**
     * @brief Exception class for bytecode loading and validation errors
     *
     * Thrown when bytecode format is invalid, corrupted, or incompatible
     * with the current runtime version.
     */
    class ByteCodeError : public std::exception {
    public:
        explicit ByteCodeError(const std::string& msg);
        const char* what() const noexcept override;
    private:
        std::string message;
    };

    /**
     * @brief Source code location information for debugging
     *
     * Maps bytecode instructions back to original source code locations.
     * This enables meaningful stack traces and debugging information.
     */
    struct SourceLocation {
        /** Character offset in the original source file */
        u32 offset;
        /** Line number in the original source file (1-based) */
        u32 line;
        /** File identifier where this code originated */
        StringId file;

        /**
         * @brief Convert to human-readable string representation
         * @return Formatted string with location details
         */
        std::string to_string() const;
    };

    enum class SymbolFlags {
        None = 0,
        Local = 1 << 0,    // определен в этом модуле
        Import = 1 << 1,   // получен из другого модуля
        Export = 1 << 2    // доступен другим модулям
    };
    
    ENUM_FLAG_OPERATORS(SymbolFlags);

    /**
     * @brief Definition of a named entity in the bytecode file
     *
     * Definitions can represent functions, global variables, constants,
     * or other named entities that are exported/imported between modules.
     */
    struct Definition {
        /** Unique name identifier within the module */
        StringId name;
        /** Type identifier (function, data, constant, etc.) */
        StringId type;
        /** Flags for the definition */
        SymbolFlags flags;
        /** Pointer to the actual data or code for this definition */
        Ptr<u8> data_ptr;

        /**
         * @brief Convert to simple string representation
         * @return Basic string representation
         */
        std::string to_string() const;

        /**
         * @brief Create detailed inspection string
         * @return Detailed formatted string for debugging
         */
        std::string inspect() const;

        inline bool has_flag(SymbolFlags flag) const {
            return (static_cast<int>(flags) & static_cast<int>(flag)) != 0;
        }
        inline void set_flag(SymbolFlags flag) {
            flags |= flag;
        }
        inline void clear_flag(SymbolFlags flag) {
            flags &= flag;
        }
    };

    /**
     * @brief Base class for entities that can be relocated in memory
     *
     * Used during module loading and dynamic linking to adjust pointers
     * when code is moved in memory.
     */
    struct Descriptor {
        /** Size of this descriptor in bytes */
        u32 desc_size;

        /**
         * @brief Adjust all internal pointers by a delta value
         * @param delta The amount to add to all pointers
         *
         * This is essential for position-independent code and dynamic
         * loading where the base address may change.
         */
        virtual void relocate_pointers(intptr_t delta) = 0;

        virtual ~Descriptor() = default;
    };

    /**
     * @brief Complete bytecode representation with code, data, and debug info
     *
     * This is the core structure representing executable code in the VM.
     * It contains the actual bytecode instructions, constant data, and
     * debugging information mapping back to source code.
     */
    struct ByteCode : public Descriptor {
        /** Number of instructions in the code section */
        u32 code_count;
        /** Size of constant data in bytes */
        u32 data_size;
        /** Number of debug information entries */
        u32 debug_count;
        /** Pointer to the instruction stream */
        Ptr<Instruction> code_ptr;
        /** Pointer to constant data section */
        Ptr<u8> data_ptr;
        /** Pointer to debug information array */
        Ptr<SourceLocation> debug_ptr;
        /** Module that owns this bytecode (set during linking) */
        Module* owner_module;

        ByteCode();

        /**
         * @brief Get pointer to the code section
         * @return Pointer to instructions or nullptr if no code
         */
        Instruction* get_code_ptr() const;

        /**
         * @brief Get pointer to the data section
         * @return Pointer to constant data or nullptr if no data
         */
        u8* get_data_ptr() const;

        /**
         * @brief Get pointer to debug information
         * @return Pointer to debug info or nullptr if no debug data
         */
        SourceLocation* get_debug_info() const;

        /**
         * @brief Find source location for a specific instruction
         * @param instruction_ip The instruction pointer/offset to lookup
         * @return Source location info or empty location if not found
         *
         * This enables mapping runtime errors back to source code
         * locations for better debugging experience.
         */
        SourceLocation find_source_location(u32 instruction_ip) const;

        /**
         * @brief Check if debug information is available
         * @return true if debug information is present
         */
        bool has_debug_info() const;

        void relocate_pointers(intptr_t delta) override;

        /**
         * @brief Create detailed inspection string
         * @return Formatted string with complete bytecode information
         */
        std::string inspect() const;
    };

    /**
     * @brief Complete binary file format for VM modules
     *
     * Represents the on-disk format for compiled modules. Contains
     * a header with validation information and a table of definitions
     * that can be functions, data, or other exported entities.
     */
    struct BinaryFile {
        // === Fixed Header (32 bytes) ===

        /** Magic number for format validation (0x00305844 'DX00' in little-endian) */
        u32 magic;
        /** Format generation/version number */
        u32 generation;
        /** Total file size in bytes */
        u32 file_size;
        /** Actually used size (may be less than file_size due to padding) */
        u32 used_size;

        // Definitions table
        /** Pointer to the definitions array */
        Ptr<Definition> definitions;
        /** Number of definitions in the table */
        u32 definitions_count;

        /** Base offset for relative pointers */
        BinaryFile* base_offset;

        /** The owner module */
        Module* owner_module;

        /** Reserved for future use */
        u32 reserved;

        // Constants
        static constexpr u32 MAGIC = ('D' | 'X' << 8 | '0' << 16 | '0' << 24);
        static constexpr u32 HEADER_SIZE = 32;
        static constexpr u32 CURRENT_GENERATION = 1;

        BinaryFile();


        BinaryFile* get_base_offset() { return  base_offset;}

        /**
         * @brief Validate the file format
         * @return true if magic number and basic structure are valid
         *
         * Performs basic sanity checks but doesn't validate all content.
         * Used during loading to reject obviously corrupt files.
         */
        bool is_valid() const;

        /**
         * @brief Get definition by index
         * @param idx Index in definitions table (0-based)
         * @return Pointer to the definition
         * @throws std::runtime_error if index is out of bounds
         */
        Definition* get_definition(u32 idx) const;

        /**
         * @brief Find definition by name
         * @param name StringId of the definition to find
         * @return Pointer to definition or nullptr if not found
         */
        Definition* find_definition_by_name(StringId name) const;

        /**
         * @brief Find bytecode by definition name
         * @param name StringId of the function to find
         * @return Pointer to ByteCode or nullptr if not found or not a function
         *
         * Specifically looks for function definitions and returns their
         * associated bytecode. Returns nullptr for non-function definitions.
         */
        ByteCode* find_bytecode_by_name(StringId name) const;

        /**
         * @brief Adjust all pointers in the file for new base address
         * @param pool_base New base address for the memory pool
         *
         * This method is called after loading the file into memory
         * to convert file offsets to valid memory pointers.
         */
        void relocate_pointers(bool to_memory = true);

        /**
          * @bried Set the owner module
          * @param Reference to the owner
          */
        void set_owner(Module* module);

        /**
         * @brief Convert to basic string representation
         * @return Formatted string with basic file information
         */
        std::string to_string() const;

        /**
         * @brief Create hex dump of the file header
         * @return Hexadecimal representation of the first 32 bytes
         *
         * Useful for debugging file format issues and corruption.
         */
        std::string hex_dump() const;

        /**
         * @brief Create detailed inspection of the entire file
         * @return Multi-line formatted string with complete file contents
         *
         * Provides a comprehensive view of all definitions and their
         * associated data for debugging and development.
         */
        std::string inspect() const;

        // ========================================================================
        // Static Factories
        // ========================================================================

        /**
        * @brief Создать BinaryFile из памяти (для выполнения)
        * @param data Данные бинарника (обычно из файла)
        * @return Указатель на готовый к выполнению BinaryFile или nullptr
        * 
        * Конвертирует смещения в реальные указатели.
        * После этого файл можно использовать для выполнения.
        */
        static BinaryFile* make_for_memory(std::vector<u8>& data) {
            BinaryFile* file = reinterpret_cast<BinaryFile*>(data.data());
            
            // сбрасываем указатель
            file->owner_module = nullptr;
            // перемещаем файл
            file->relocate_pointers(true);
            // проверяем валидность
            if (!file->is_valid()) {
                lg::error("Invalid binary file");
                return nullptr;
            }
            
            return file;
        }
        
        /**
        * @brief Подготовить BinaryFile для записи в файл
        * @return true если успешно
        * 
        * Конвертирует реальные указатели в смещения.
        * После этого файл можно сохранять.
        */
        bool make_for_file() {
            relocate_pointers(false);
            return true;
        }
        
        /**
        * @brief Загрузить BinaryFile из файла
        * @param filename Имя файла
        * @param out_data Выходной буфер с данными
        * @return BinaryFile* Готовый к выполнению файл или nullptr
        */
        static BinaryFile* load_from_file(const std::string& filename, std::vector<u8>& out_data) {
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                lg::error("Cannot open file: {}", filename);
                return nullptr;
            }
            
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            out_data.resize(size);
            file.read(reinterpret_cast<char*>(out_data.data()), size);
            file.close();
            
            return make_for_memory(out_data);
        }
        
        /**
        * @brief Сохранить BinaryFile в файл
        * @param filename Имя файла
        * @return true если успешно
        */
        bool save_to_file(const std::string& filename) {
            make_for_file();
            
            std::ofstream file(filename, std::ios::binary);
            if (!file) {
                lg::error("Cannot create file: {}", filename);
                relocate_pointers(false);
                return false;
            }
            
            file.write(reinterpret_cast<const char*>(this), file_size);
            file.close();
            
                relocate_pointers(true);
            return true;
        }

        private:

        void apply_delta_to_pointers(ptrdiff_t delta);     
    };

} // namespace runtime::files