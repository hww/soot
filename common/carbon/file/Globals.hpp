#pragma once

#include "common/carbon/lib/StringId.hpp"
#include "file/BinaryFile.hpp"
#include "file/DCHeader.hpp"

#include "fmt/format.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace carbon {

    /// @brief One exported symbol registered in the global symbol table.
    /// @details Created by Globals::register_symbols_from_file for each DCEntry
    ///          whose m_nameID is non-zero. The pointer points into the owning
    ///          BinaryFile's mapped bytes and is invalidated on unload.
    struct Symbol {
        void    *ptr = nullptr;   ///< pointer to the symbol payload inside the owning module
        StringId typeId;          ///< SID of the symbol type (validated at call sites)
        StringId ownerModulePath; ///< module that owns this symbol; used for unload cleanup
    };

    /// @brief Process-wide symbol registry and module cache.
    /// @details Owns every loaded BinaryFile and exposes a flat name -> Symbol map
    ///          for O(1) lookup. Thread-safety: none; callers must synchronise.
    class Globals {
    public:
        /// @return the process-wide singleton instance.
        static Globals &inst() {
            static Globals instance;
            return instance;
        }

        Globals(const Globals &) = delete;
        Globals &operator=(const Globals &) = delete;

        /// @return a snapshot of all currently registered symbol names.
        std::vector<StringId> all_symbols() const {
            std::vector<StringId> result;
            result.reserve(m_symbols.size());
            for (const auto &[k, v] : m_symbols) { result.push_back(k); }
            return result;
        }

        /// @brief Load a DC module from disk and register its symbols.
        /// @return true on success, false on I/O or parse failure.
        bool load_module(const std::filesystem::path &path) {
            const StringId module_path_id(path.string());
            if (m_modules.contains(module_path_id)) { return true; }

            auto result = BinaryFile::from_path(path);
            if (!result.has_value()) {
                std::cerr << "[Globals] load_module: failed to read '" << path.string()
                          << "': " << result.error() << "\n";
                return false;
            }

            BinaryFile &file = m_modules.emplace(module_path_id, std::move(*result)).first->second;
            return register_symbols_from_file(file, module_path_id);
        }

        /// @brief Register a BinaryFile that was already loaded into memory.
        /// @details The caller transfers ownership. Symbols are registered before
        ///          the move so that entry pointers remain valid.
        /// @return true on success, false if symbol registration failed.
        bool load_module(BinaryFile &&file) {
            const std::string module_name = fmt::format("module_{}", m_modules.size());
            const StringId    module_path_id(module_name);

            if (!register_symbols_from_file(file, module_path_id)) { return false; }

            m_modules.emplace(module_path_id, std::move(file));
            return true;
        }

        /// @brief Remove a module and drop all symbols owned by it.
        void unload_module(const std::string &path) {
            const StringId module_path_id(path);
            if (!m_modules.contains(module_path_id)) { return; }

            std::erase_if(m_symbols, [module_path_id](const auto &item) {
                return item.second.ownerModulePath == module_path_id;
            });

            m_modules.erase(module_path_id);
            std::cout << "[Globals] Module " << path << " unloaded." << std::endl;
        }

        /// @brief Fast-path lookup; returns nullptr if the symbol is missing.
        [[nodiscard]] inline void *find_symbol_ptr(StringId name) const noexcept {
            const auto it = m_symbols.find(name);
            return it != m_symbols.end() ? it->second.ptr : nullptr;
        }

        /// @brief Look up a symbol and verify its type.
        /// @return the pointer cast to T*, or nullptr if missing or wrong type.
        template <typename T>
        [[nodiscard]] T *get_as(StringId name, StringId expectedType) const noexcept {
            const auto it = m_symbols.find(name);
            if (it == m_symbols.end() || it->second.typeId != expectedType) { return nullptr; }
            return static_cast<T *>(it->second.ptr);
        }

        /// @brief Drop all symbols and unload all modules.
        void clear_all() {
            m_symbols.clear();
            m_modules.clear();
        }

        /// @brief Typed lookup with optional exception on failure.
        /// @return the symbol pointer, or nullptr if not found / wrong type (when !throw_error).
        void *lookup(StringId name, StringId type_id, bool throw_error = false) {
            const auto it = m_symbols.find(name);
            if (it != m_symbols.end()) {
                if (it->second.typeId == type_id) { return it->second.ptr; }
                if (throw_error) {
                    throw std::runtime_error(
                        fmt::format("Expected global {} with type {}, found {}", name.to_cstring(),
                                    type_id.to_cstring(), it->second.typeId.to_cstring()));
                }
            }
            if (throw_error) {
                throw std::runtime_error(fmt::format("Undefined global {}", name.to_cstring()));
            }
            return nullptr;
        }

    private:
        Globals() = default;

        /// @brief Register all named entries from a loaded module into the global
        ///        symbol table.
        /// @details Iterates the entry table via BinaryFile::entries() rather than
        ///          poking m_pStartOfData directly. This guarantees that any
        ///          defensive fixups performed by BinaryFile (e.g. header pointer
        ///          relocation) are honoured here as well.
        ///
        ///          Entries with nameID == 0 are skipped (unnamed / anonymous).
        bool register_symbols_from_file(const BinaryFile &file, const StringId &module_path_id) {
            const DC_Header *header = file.m_dcheader;
            if (!header) {
                std::cerr << "[Globals] register_symbols: null header\n";
                return false;
            }

            // Route through BinaryFile::entries() so that header pointer fixups
            // and any future relocation logic are applied consistently.
            const DCEntry *table = file.entries();
            if (!table) {
                std::cerr << "[Globals] register_symbols: null entry table\n";
                return false;
            }

            for (u32 i = 0; i < header->m_numEntries; ++i) {
                const DCEntry &entry = table[i];
                if (entry.m_nameID == 0) { continue; }
                m_symbols[StringId(entry.m_nameID)] = Symbol{
                    const_cast<void *>(entry.m_entryPtr), StringId(entry.m_typeId), module_path_id};
            }
            return true;
        }

        std::unordered_map<StringId, Symbol>     m_symbols; ///< flat name -> symbol map
        std::unordered_map<StringId, BinaryFile> m_modules; ///< owning module cache (RAII)
    };

} // namespace carbon