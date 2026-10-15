#pragma once

#include "carbon/lib/StringId.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace carbon {

    /// @brief One exported symbol in the global symbol index.
    ///
    /// @details `ptr` is the address of the symbol's slot inside its
    ///          owning module's mapped bytes. It is NOT the value of
    ///          the symbol. Globals is an index, not a storage.
    ///
    ///          The slot is valid exactly as long as the owning module
    ///          stays loaded. Globals does NOT own BinaryFile; that is
    ///          Carbon::ModuleRegistry's job.
    struct Symbol {
        void    *ptr = nullptr;
        StringId typeId;
        StringId ownerModulePath;
    };

    /// @brief Flat SID -> Symbol index across all loaded modules.
    ///
    /// @details Globals answers one question: "for a given SID, in
    ///          which loaded module does a slot with that name live,
    ///          and at what address?"
    ///
    ///          It owns no values, holds no BinaryFile, and does not
    ///          know about the DC file format. It is a plain hash map
    ///          with a handful of operations on it.
    class Globals {
    public:
        Globals() = default;
        ~Globals() = default;
        Globals(const Globals &) = delete;
        Globals &operator=(const Globals &) = delete;

        // ---- Primitives ----

        /// @brief Register or overwrite a single symbol.
        void register_symbol(StringId name, void *ptr, StringId type, StringId owner_module) {
            m_symbols[name] = Symbol{ptr, type, owner_module};
        }

        /// @brief Remove a single symbol by name.
        void remove_symbol(StringId name) { m_symbols.erase(name); }

        /// @brief Remove every symbol whose owner is `module_path_id`.
        /// @return Number of symbols removed.
        std::size_t remove_module_symbols(const StringId &module_path_id) {
            return std::erase_if(m_symbols, [&](const auto &item) {
                return item.second.ownerModulePath == module_path_id;
            });
        }

        /// @brief Drop every symbol.
        void clear() { m_symbols.clear(); }

        // ---- Lookup ----

        [[nodiscard]] inline void *find_symbol_ptr(StringId name) const noexcept {
            const auto it = m_symbols.find(name);
            return it != m_symbols.end() ? it->second.ptr : nullptr;
        }

        [[nodiscard]] std::vector<StringId> all_symbols() const {
            std::vector<StringId> result;
            result.reserve(m_symbols.size());
            for (const auto &[k, v] : m_symbols) { result.push_back(k); }
            return result;
        }

        template <typename T>
        [[nodiscard]] T *get_as(StringId name, StringId expectedType) const noexcept {
            const auto it = m_symbols.find(name);
            if (it == m_symbols.end() || it->second.typeId != expectedType) { return nullptr; }
            return static_cast<T *>(it->second.ptr);
        }

    private:
        std::unordered_map<StringId, Symbol> m_symbols;
    };

} // namespace carbon