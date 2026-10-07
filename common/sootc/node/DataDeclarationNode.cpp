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
#include <set>

// ============================================================================
// Helper: is this a builtin scalar type we can write directly?
// ============================================================================
namespace {
    /// @brief Return true if `name` is a builtin scalar type that can be
    ///        serialised by simply copying its value into the payload.
    /// @details These are the leaf types in the builtin type tree:
    ///            int8/int16/int32/int64
    ///            uint8/uint16/uint32/uint64
    ///            float
    ///            bool
    ///            symbol
    ///          Everything else (structures, arrays, inline objects) needs
    ///          a different handling path.
    bool is_builtin_scalar(const std::string &name) {
        static const std::set<std::string> kNames = {
            "int8", "i8",    "uint8", "u8",     "int16",   "i16",    "uint16", "u16",
            "int",  "int32", "i32",   "uint32", "u32",     "int64",  "i64",    "uint64",
            "u64",  "float", "f32",   "bool",   "boolean", "symbol", "sid64",
        };
        return kNames.count(name) > 0;
    }
} // namespace

namespace sootc {

    ProgramBinaryElement DataDeclarationNode::generate(GlobalState &state) {
        (void)state;

        if (!m_instance) {
            lg::warn("DataDeclarationNode '{}' has no instance", m_name);
            return ProgramBinaryElement(0);
        }

        // Only static initialization is valid at top level.
        if (!m_instance->is_static()) {
            lg::error("DataDeclarationNode '{}': only 'static' allocation is "
                      "valid at top level, got '{}'",
                      m_name, m_instance->allocation());
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

            // Find the init for this field, if any.
            const NewNode::FieldInit *init = nullptr;
            for (const auto &f : m_instance->fields()) {
                if (f.name == field_name) {
                    init = &f;
                    break;
                }
            }

            // No init: leave as zero.
            if (!init || !init->value) { continue; }

            // Only ConstNode is supported for now. Anything else (arithmetic,
            // function call) would require compile-time evaluation, which we
            // do not do yet.
            const auto *cn = dynamic_cast<const ConstNode *>(init->value.get());
            if (!cn) {
                lg::warn("DataDeclarationNode '{}': field '{}' has non-const init ({}), skipped",
                         m_name, field_name, init->value->node_type());
                continue;
            }

            // Resolve the field's type from TypeSystem.
            Type *field_type = ts.lookup_type_no_throw(field.type().base_type());
            if (!field_type) {
                lg::warn("DataDeclarationNode '{}': field '{}' has unknown type '{}', skipped",
                         m_name, field_name, field.type().print());
                continue;
            }

            const std::string ft = field.type().base_type();
            const size_t      off = static_cast<size_t>(field_offset);
            const size_t      field_size = static_cast<size_t>(ts.get_size_in_type(field));

            if (off + field_size > payload.size()) {
                lg::warn("DataDeclarationNode '{}': field '{}' at offset {} size {} exceeds "
                         "payload size {}",
                         m_name, field_name, off, field_size, payload.size());
                continue;
            }

            // ---- Builtin scalar: write the value directly. ----
            if (is_builtin_scalar(ft)) {
                // int / uint / bool: write as integer, of the appropriate width.
                if (ft == "float" || ft == "f32") {
                    const f32 v = static_cast<f32>(cn->float_value());
                    std::memcpy(payload.data() + off, &v, sizeof(f32));
                } else if (ft == "symbol" || ft == "sid64") {
                    const sid64 v = StringId(cn->string_value()).value;
                    std::memcpy(payload.data() + off, &v, sizeof(sid64));
                } else if (ft == "bool" || ft == "boolean") {
                    const u8 v = cn->int_value() ? 1 : 0;
                    payload[off] = static_cast<std::byte>(v);
                } else {
                    // Integer of some width.
                    const i64 v = cn->int_value();
                    switch (field_size) {
                    case 1: {
                        const u8 b = static_cast<u8>(v);
                        std::memcpy(payload.data() + off, &b, 1);
                        break;
                    }
                    case 2: {
                        const u16 b = static_cast<u16>(v);
                        std::memcpy(payload.data() + off, &b, 2);
                        break;
                    }
                    case 4: {
                        const u32 b = static_cast<u32>(v);
                        std::memcpy(payload.data() + off, &b, 4);
                        break;
                    }
                    case 8: {
                        const u64 b = static_cast<u64>(v);
                        std::memcpy(payload.data() + off, &b, 8);
                        break;
                    }
                    default:
                        lg::warn("DataDeclarationNode '{}': field '{}' has unsupported "
                                 "integer width {}",
                                 m_name, field_name, field_size);
                        break;
                    }
                }
                continue;
            }

            // ---- Non-builtin: structure, inline, array. Not yet supported. ----
            lg::warn("DataDeclarationNode '{}': field '{}' type '{}' is not a builtin scalar "
                     "(size {}); skipping",
                     m_name, field_name, ft, field_size);
        }

        // 5. Build ProgramBinaryElement.
        ProgramBinaryElement element(static_cast<u64>(payload.size()));

        element.m_entry = {.m_nameID = StringId(m_name).value,
                           .m_typeId = StringId(type_name).value,
                           .m_entryPtr = nullptr};

        // 6. Attach the struct layout so downstream tools can decode the payload.
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

        lg::info("DataDeclarationNode '{}': {} bytes, type '{}', exported={}", m_name, size_bytes,
                 type_name, m_exported);

        return element;
    }

} // namespace sootc