#pragma once
#include "common/CommonTypes.hpp"
#include "common/carbon/ForwardDeclarations.hpp"
#include "common/carbon/files/Base.hpp"  
#include "common/carbon/files/FunctionDesc.hpp"  
#include "common/carbon/lib/Ptr.hpp"  
#include "common/util/Log.hpp"
#include <vector>
#include <fstream>
#include <cassert>

using namespace carbon::lib;
using namespace carbon::vm;
using namespace carbon::modules;

namespace carbon::files {


    /**
     * @brief Definition of a named entity in the FunctionDesc file
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
        Ptr<u8> data;

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
        /**
         * Every defition points to descriptor
         */
        void relocate_pointers(intptr_t delta) {
            data.offset += delta;
            auto desc = reinterpret_cast<Descriptor*>(data.ptr);
            if (desc)
                desc->relocate_pointers(delta);
        }
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
         * @brief Find FunctionDesc by definition name
         * @param name StringId of the function to find
         * @return Pointer to FunctionDesc or nullptr if not found or not a function
         *
         * Specifically looks for function definitions and returns their
         * associated FunctionDesc. Returns nullptr for non-function definitions.
         */
        FunctionDesc* find_function_by_name(StringId name) const;

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

} // namespace carbon::files