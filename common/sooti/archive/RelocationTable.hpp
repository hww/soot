
// RelocationTable.hpp
#pragma once

#include "common/sooti/Archive.hpp"
#include "common/sooti/Object.hpp"
#include "common/sooti/Printer.hpp"
#include "fmt/format.h"
#include <string>
#include <vector>

namespace script {

class RelocationTable : public NativeObject {
  public:
    enum class RelocType : uint8_t {
        ABS_ADDR = 0,   // Абсолютный адрес
        SYMBOL_CRC = 1, // CRC символа
        RELATIVE = 2,   // Относительный адрес
        SYMBOL_TABLE_REF = 3
    };

    struct Relocation {
        size_t      offset;
        RelocType   type;
        std::string target_name;

        void serialize(Archive &ar) {
            if (ar.is_reading()) {
                CompactIndex off;
                ar << off;
                offset = off.value;

                uint8_t t;
                ar << t;
                type = static_cast<RelocType>(t);

                CompactIndex name_len;
                ar << name_len;
                target_name.resize(name_len.value);
                ar.serialize_obj(&target_name[0], name_len.value);
            } else {
                CompactIndex off(offset);
                ar << off;

                uint8_t t = static_cast<uint8_t>(type);
                ar << t;

                CompactIndex name_len(target_name.length());
                ar << name_len;
                ar.serialize_obj(&target_name[0], target_name.length());
            }
        }
    };

    RelocationTable() = default;

    // ============================================================
    // HeapObject implementation
    // ============================================================

    Object get_at(const Object &key) override;
    void   set_at(const Object &key, const Object &value) override;

    std::string class_name() const override {
        return "relocation-table";
    }

    std::string full_class_name() const override {
        return "RelocationTable";
    }

    Object type_name_obj() const override {
        return Object::make_symbol(class_name());
    }

    bool is_class_name(const Object &name) const override {
        return name == RelocationTable::type_name_obj() || NativeObject::is_class_name(name);
    }

    std::string print() const override {
        return fmt::format("#<relocation-table {} relocs>", m_relocations.size());
    }

    Object inspect() const override {
        return pretty_print::build_list(
            pretty_print::build_list(Object::make_symbol(":type"),
                                     Object::make_string(class_name())),
            pretty_print::build_list(Object::make_symbol(":count"),
                                     Object::make_integer(m_relocations.size())));
    }

    // ============================================================
    // Serialization
    // ============================================================

    void serialize(Archive &ar) override {
        CompactCrc32 magic{0x52454C4F}; // "RELO"
        ar << magic;

        if (ar.is_reading()) {
            CompactIndex count;
            ar << count;

            m_relocations.clear();
            m_relocations.reserve(count.value);

            for (int i = 0; i < count.value; i++) {
                Relocation reloc;
                reloc.serialize(ar);
                m_relocations.push_back(std::move(reloc));
            }
        } else {
            CompactIndex count(m_relocations.size());
            ar << count;

            for (auto &reloc : m_relocations) {
                reloc.serialize(ar);
            }
        }
    }

    inline friend Archive &operator<<(Archive &ar, RelocationTable &v) {
        v.serialize(ar);
        return ar;
    }

    // Для shared_ptr
    inline friend Archive &operator<<(Archive &ar, const std::shared_ptr<RelocationTable> &ptr) {
        if (ptr) {
            ptr->serialize(ar);
        } else {
            // Сериализуем nullptr как нулевую магию
            CompactPointer null_magic{0x00000000};
            ar << null_magic;
        }
        return ar;
    }

    // ============================================================
    // Public API
    // ============================================================

    void add(size_t offset, RelocType type, const std::string &target) {
        m_relocations.push_back({offset, type, target});
    }

    const std::vector<Relocation> &get() const {
        return m_relocations;
    }

    void clear() {
        m_relocations.clear();
    }

  private:
    std::vector<Relocation> m_relocations;
};

} // namespace script
