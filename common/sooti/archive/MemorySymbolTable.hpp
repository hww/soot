#include "Archive.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Printer.hpp"
#include "common/util/Crc32.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace script {

class MemorySymbolTable : public HeapObject {
  private:
    struct SymbolEntry {
        std::string name;
        uint32_t    crc32;
        uint32_t    string_offset; // смещение в string pool

        SymbolEntry() : crc32(0), string_offset(0) {}
        SymbolEntry(const std::string &n)
            : name(n), crc32(util::compute_crc32(n)), string_offset(0) {}
        ~SymbolEntry() = default;

        void serialize(Archive &ar) {
            if (ar.is_loading()) {
                CompactIndex name_len;
                ar << name_len;

                // Читаем имя
                name.resize(name_len.value);            // неявное преобразование
                ar.serialize(&name[0], name_len.value); // CompactIndex в int

                // Вычисляем CRC заново
                crc32 = util::compute_crc32(name);
                string_offset = 0;
            } else {
                // При сохранении пишем имя
                CompactIndex name_len(name.length());
                ar << name_len;
                ar.serialize(&name[0], name.length());
            }
        }
    };

    std::vector<SymbolEntry>             m_symbols;
    std::string                          m_string_pool;
    std::unordered_map<uint32_t, size_t> m_crc_to_index;

  public:
    // ============================================================
    // HeapObject implementation
    // ============================================================

    std::string class_name() const override {
        return "memory-symbol-table";
    }

    std::string full_class_name() const override {
        return "MemorySymbolTable";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == type_name_obj() || HeapObject::is_class_name(name);
    }

    std::string print() const override {
        return fmt::format("#<memory-symbol-table {} symbols>", m_symbols.size());
    }

    Object inspect() const override {
        return pretty_print::build_list(
            pretty_print::build_list(Object::make_symbol(":type"),
                                     Object::make_string(class_name())),
            pretty_print::build_list(Object::make_symbol(":count"),
                                     Object::make_integer(m_symbols.size())));
    }

    // ============================================================
    // Serialization
    // ============================================================

    void serialize(Archive &ar) override {
        Crc32Value magic{0x53594D54}; // "SYMT"
        ar << magic;

        if (ar.is_loading()) {
            CompactIndex count;
            ar << count;

            m_symbols.clear();
            m_symbols.reserve(count.value); // исправлено

            for (int i = 0; i < count.value; i++) { // исправлено
                SymbolEntry sym;
                sym.serialize(ar);
                m_symbols.push_back(std::move(sym));
            }

            rebuild_index();
            rebuild_string_pool();

        } else {
            CompactIndex count(m_symbols.size());
            ar << count;

            for (auto &sym : m_symbols) {
                sym.serialize(ar);
            }
        }
    }

    // ============================================================
    // Public API
    // ============================================================

    size_t add_symbol(const std::string &name) {
        uint32_t crc = util::compute_crc32(name);

        auto it = m_crc_to_index.find(crc);
        if (it != m_crc_to_index.end()) {
            return it->second;
        }

        size_t index = m_symbols.size();
        m_symbols.emplace_back(name);
        m_crc_to_index[crc] = index;

        return index;
    }

    std::optional<size_t> find_by_crc32(uint32_t crc) const {
        auto it = m_crc_to_index.find(crc);
        if (it != m_crc_to_index.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::string get_name(size_t index) const {
        if (index >= m_symbols.size())
            return "";
        return m_symbols[index].name;
    }

    uint32_t get_crc32(size_t index) const {
        if (index >= m_symbols.size())
            return 0;
        return m_symbols[index].crc32;
    }

    size_t size() const {
        return m_symbols.size();
    }
    bool empty() const {
        return m_symbols.empty();
    }

  private:
    void rebuild_index() {
        m_crc_to_index.clear();
        for (size_t i = 0; i < m_symbols.size(); i++) {
            m_crc_to_index[m_symbols[i].crc32] = i;
        }
    }

    void rebuild_string_pool() {
        m_string_pool.clear();
        for (auto &sym : m_symbols) {
            sym.string_offset = m_string_pool.size();
            m_string_pool.append(sym.name);
            m_string_pool.push_back('\0');
        }
    }

    // Друзья для операторов Archive
    friend Archive &operator<<(Archive &ar, MemorySymbolTable &v);
    friend Archive &operator<<(Archive &ar, const std::shared_ptr<MemorySymbolTable> &ptr);
};

// ============================================================
// Операторы для Archive
// ============================================================

inline Archive &operator<<(Archive &ar, MemorySymbolTable &v) {
    v.serialize(ar);
    return ar;
}

inline Archive &operator<<(Archive &ar, const std::shared_ptr<MemorySymbolTable> &ptr) {
    if (ptr) {
        ptr->serialize(ar);
    } else {
        Crc32Value null_magic{0};
        ar << null_magic;
    }
    return ar;
}

} // namespace script