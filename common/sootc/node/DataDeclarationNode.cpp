#include "DataDeclarationNode.hpp"
#include "ConstNode.hpp"

#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/StringIdManager.hpp"
#include "common/type_system/Type.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/util/Log.hpp"

#include "fmt/format.h"

#include <cstring>
#include <vector>

namespace sootc {

    ProgramBinaryElement DataDeclarationNode::generate(GlobalState &state) {
        (void)state;

        if (!m_instance) {
            lg::warn("DataDeclarationNode '{}' has no instance", m_name);
            return ProgramBinaryElement(0);
        }

        // 1. Look up the type.
        TypeSystem        &ts = TypeSystem::instance();
        const std::string &type_name = m_instance->type().base_type();
        Type              *type = ts.lookup_type_no_throw(type_name);
        if (!type) {
            lg::error("DataDeclarationNode '{}': unknown type '{}'", m_name, type_name);
            return ProgramBinaryElement(0);
        }

        // 2. Must be a StructureType.
        auto *st = dynamic_cast<StructureType *>(type);
        if (!st) {
            lg::error("DataDeclarationNode '{}': type '{}' is not a structure", m_name, type_name);
            return ProgramBinaryElement(0);
        }

        const int size_bytes = st->get_size_in_memory();
        if (size_bytes <= 0 || size_bytes > 0x10000) {
            lg::error("DataDeclarationNode '{}': invalid size {} for type '{}'", m_name, size_bytes,
                      type_name);
            return ProgramBinaryElement(0);
        }

        // 3. Zero-initialized payload.
        std::vector<std::byte> payload(static_cast<size_t>(size_bytes), std::byte{0});

        // 4. Fill in fields from m_instance->fields().
        for (const auto &field : st->fields()) {
            const std::string &field_name = field.name();
            const int          field_offset = field.offset();

            const NewNode::FieldInit *init = nullptr;
            for (const auto &f : m_instance->fields()) {
                if (f.name == field_name) {
                    init = &f;
                    break;
                }
            }

            if (!init || !init->value) { continue; }

            const auto *cn = dynamic_cast<const ConstNode *>(init->value.get());
            if (!cn) {
                lg::warn("DataDeclarationNode '{}': field '{}' has non-const init ({}), skipped",
                         m_name, field_name, init->value->node_type());
                continue;
            }

            const std::string &ft = field.type().base_type();
            const size_t       off = static_cast<size_t>(field_offset);

            if (ft == "float" || ft == "f32") {
                const f32 v = static_cast<f32>(cn->float_value());
                if (off + sizeof(f32) <= payload.size())
                    std::memcpy(payload.data() + off, &v, sizeof(f32));
            } else if (ft == "int" || ft == "int32") {
                const i32 v = static_cast<i32>(cn->int_value());
                if (off + sizeof(i32) <= payload.size())
                    std::memcpy(payload.data() + off, &v, sizeof(i32));
            } else if (ft == "int64") {
                const i64 v = cn->int_value();
                if (off + sizeof(i64) <= payload.size())
                    std::memcpy(payload.data() + off, &v, sizeof(i64));
            } else if (ft == "uint8") {
                const u8 v = static_cast<u8>(cn->int_value());
                if (off + sizeof(u8) <= payload.size())
                    std::memcpy(payload.data() + off, &v, sizeof(u8));
            } else if (ft == "bool" || ft == "boolean") {
                const u8 v = cn->int_value() ? 1 : 0;
                if (off + 1 <= payload.size()) payload[off] = static_cast<std::byte>(v);
            } else if (ft == "uint64") {
                const u64 v = static_cast<u64>(cn->int_value());
                if (off + sizeof(u64) <= payload.size())
                    std::memcpy(payload.data() + off, &v, sizeof(u64));
            } else if (ft == "symbol" || ft == "sid64") {
                const sid64 v = StringId(cn->string_value()).value;
                std::memcpy(payload.data() + off, &v, sizeof(sid64));
            } else {
                lg::warn("DataDeclarationNode '{}': field '{}' type '{}' not yet supported", m_name,
                         field_name, ft);
            }
        }

        // 5. Build ProgramBinaryElement.
        ProgramBinaryElement element(static_cast<u64>(payload.size()));

        element.m_entry = {.m_nameID = StringId(m_name).value,
                           .m_typeId = StringId(type_name).value,
                           .m_entryPtr = nullptr};

        // 6. Attach the struct layout so downstream tools (BinaryFileInspector)
        //    can decode the payload WITHOUT knowing about TypeSystem.
        //    This mirrors how SsType entries are self-describing in the file.
        StructLayoutInfo layout;
        layout.type_name = type_name;
        layout.total_size = static_cast<u32>(size_bytes);
        for (const auto &field : st->fields()) {
            StructFieldInfo fi;
            fi.name = field.name();
            fi.type_name = field.type().print();
            fi.offset = static_cast<u32>(field.offset());
            fi.size = static_cast<u32>(ts.get_size_in_type(field));
            fi.is_inline = field.is_inline();
            fi.is_array = field.is_array();
            fi.array_size = field.array_size();
            layout.fields.push_back(std::move(fi));
        }
        element.m_structLayout = std::move(layout);

        // Push payload — no relocations yet.
        element.push_blob(payload.data(), payload.size(), /*relocation_bit=*/0);

        lg::info("DataDeclarationNode '{}': {} bytes, type '{}', exported={} m_structLayout has {} fields", m_name, size_bytes,
                 type_name, m_exported, element.m_structLayout ? element.m_structLayout->fields.size() : 0);
        return element;
    }

} // namespace sootc