#include "DataDeclarationNode.hpp"
#include "sootc/node/ConstFold.hpp"
#include "sootc/node/ConstNode.hpp"

#include "carbon/lib/StringId.hpp"
#include "carbon/lib/StringIdManager.hpp"
#include "type_system/Type.hpp"
#include "type_system/TypeSpec.hpp"
#include "type_system/TypeSystem.hpp"
#include "util/Log.hpp"

#include "fmt/format.h"

#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

// ============================================================================
// Helpers
// ============================================================================
namespace {

    /// @brief Return true if `name` is a builtin scalar type that can be
    ///        serialised by simply copying its value into the payload.
    ///
    /// @details These are the leaf types in the builtin type tree:
    ///            int8/int16/int32/int64
    ///            uint8/uint16/uint32/uint64
    ///            float
    ///            bool
    ///            symbol
    ///          Everything else (structures, arrays, inline objects)
    ///          goes through a different path.
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

    namespace {

        /// @brief Write one scalar value at `off` inside `payload`.
        ///
        /// @details The width is taken from the field type, not from the
        ///          ConstNode. So an `int32` field gets 4 bytes even if
        ///          the constant was an i64.
        ///
        /// @throws std::runtime_error if the scalar type is not one of
        ///         the supported builtin kinds, or the width is unknown.
        void write_scalar(std::vector<std::byte> &payload, size_t off, const ConstNode *cn,
                          const std::string &type_name, size_t width) {
            if (off + width > payload.size()) {
                throw std::runtime_error(
                    fmt::format("scalar at offset {} size {} exceeds payload size {}", off, width,
                                payload.size()));
            }

            if (type_name == "float" || type_name == "f32") {
                const f32 v = static_cast<f32>(cn->float_value());
                std::memcpy(payload.data() + off, &v, sizeof(f32));
                return;
            }
            if (type_name == "symbol" || type_name == "sid64") {
                // The value for a symbol is stored in the ConstNode as a
                // string (the symbol name) and hashed here. This matches
                // the old behaviour.
                const sid64 v = StringId(cn->string_value()).value;
                std::memcpy(payload.data() + off, &v, sizeof(sid64));
                return;
            }
            if (type_name == "bool" || type_name == "boolean") {
                const u8 v = cn->int_value() ? 1 : 0;
                payload[off] = static_cast<std::byte>(v);
                return;
            }

            // Integer of some width.
            const i64 v = cn->int_value();
            switch (width) {
            case 1: {
                const u8 b = static_cast<u8>(v);
                std::memcpy(payload.data() + off, &b, 1);
                return;
            }
            case 2: {
                const u16 b = static_cast<u16>(v);
                std::memcpy(payload.data() + off, &b, 2);
                return;
            }
            case 4: {
                const u32 b = static_cast<u32>(v);
                std::memcpy(payload.data() + off, &b, 4);
                return;
            }
            case 8: {
                const u64 b = static_cast<u64>(v);
                std::memcpy(payload.data() + off, &b, 8);
                return;
            }
            default: throw std::runtime_error(fmt::format("unsupported integer width {}", width));
            }
        }

        /// @brief Find the (name -> init) entry for a given field inside
        ///        a NewNode's keyword-args list.
        ///
        /// @return pointer to the FieldInit, or nullptr if the field was
        ///         not initialized.
        const NewNode::FieldInit *find_field_init(const NewNode *node, const std::string &name) {
            if (!node) { return nullptr; }
            for (const auto &fi : node->fields()) {
                if (fi.name == name) { return &fi; }
            }
            return nullptr;
        }

        /// @brief Recursively write a compile-time value at `base_off`.
        ///
        /// @param payload    Byte buffer, already zeroed, exact size.
        /// @param base_off   Offset where this value starts inside `payload`.
        /// @param type       Static type of the value being written.
        /// @param init       AST node that produced the value. Must be
        ///                   either a ConstNode (for scalars) or a
        ///                   static NewNode (for structures).
        /// @param ts         TypeSystem, for resolving subfields.
        ///
        /// @details Supported cases, in order of dispatch:
        ///
        ///            1. Builtin scalar        — try_fold_const + memcpy.
        ///            2. Inline structure      — recurse for each subfield.
        ///            3. Inline array          — TODO (next step).
        ///            4. Pointer to another    — TODO (deferred relocation).
        ///               static or heap object
        ///
        ///          Anything else throws with a message naming the exact
        ///          field and the offending node type. This is what makes
        ///          the "unsupported case" visible at compile time instead
        ///          of silently writing zeros.
        void write_value(std::vector<std::byte> &payload, size_t base_off, Type *type,
                         const ExpressionNode *init, TypeSystem &ts) {
            const std::string &type_name = type->get_name();

            // ---- 1. Builtin scalar ----
            if (is_builtin_scalar(type_name)) {
                auto cn = try_fold_const(init);
                if (!cn) {
                    throw std::runtime_error(
                        fmt::format("scalar field of type '{}' has non-constant initializer '{}'",
                                    type_name, init ? init->to_string() : "<null>"));
                }
                const size_t width = static_cast<size_t>(type->get_size_in_memory());
                write_scalar(payload, base_off, cn.get(), type_name, width);
                return;
            }

            // ---- 2. Inline structure ----
            if (auto *st = dynamic_cast<StructureType *>(type)) {
                auto *new_node = dynamic_cast<const NewNode *>(init);
                if (!new_node || !new_node->is_static()) {
                    throw std::runtime_error(fmt::format("field of type '{}' needs a compile-time "
                                                         "(new 'static '{} ...) initializer",
                                                         type_name, type_name));
                }
                for (const auto &sub : st->fields()) {
                    const NewNode::FieldInit *sub_init = find_field_init(new_node, sub.name());
                    if (!sub_init || !sub_init->value) { continue; } // leave zero

                    Type *sub_type = ts.lookup_type_no_throw(sub.type().base_type());
                    if (!sub_type) {
                        throw std::runtime_error(
                            fmt::format("unknown subfield type '{}' for field '{}.{}'",
                                        sub.type().print(), type_name, sub.name()));
                    }

                    // Sub-fields are placed at (base_off + subfield.offset).
                    // The subfield's offset is measured from the start of
                    // `type`, so adding base_off shifts it into `payload`.
                    write_value(payload, base_off + static_cast<size_t>(sub.offset()), sub_type,
                                sub_init->value.get(), ts);
                }
                return;
            }

            // ---- Unsupported (for now) ----
            throw std::runtime_error(
                fmt::format("field of type '{}' is not yet supported in static "
                            "initialization (inline array, pointer, or dynamic "
                            "array — implement in a later step)",
                            type_name));
        }

    } // namespace

    ProgramBinaryElement DataDeclarationNode::generate(StringsTable &state) {
        (void)state;

        if (!m_instance) {
            lg::warn("DataDeclarationNode '{}' has no instance", m_name);
            return ProgramBinaryElement(0);
        }

        // Heap-allocated declarations have no compile-time payload.
        // The slot in the file is a placeholder (8 zero bytes), emitted
        // by FileNode::make_binary from FileNode::placeholders(). The
        // actual pointer is written at runtime by a StoreGlobalNode that
        // lives in the top-level body.
        if (m_heap) {
            lg::debug("DataDeclarationNode '{}': heap, no payload", m_name);
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

        // 4. Recursively write each field that was initialized.
        //
        // The recursion is delegated to write_value(). Each top-level
        // field of `st` is written at its own offset; inline structures
        // are expanded further inside write_value().
        //
        // Errors thrown by write_value carry the field name; we wrap
        // them with the DataDeclarationNode name so the final message
        // identifies the exact define and field.
        for (const auto &field : st->fields()) {
            const NewNode::FieldInit *init = find_field_init(m_instance.get(), field.name());
            if (!init || !init->value) { continue; } // leave zero

            Type *field_type = ts.lookup_type_no_throw(field.type().base_type());
            if (!field_type) {
                lg::warn("DataDeclarationNode '{}': field '{}' has unknown type '{}', skipped",
                         m_name, field.name(), field.type().print());
                continue;
            }

            try {
                write_value(payload, static_cast<size_t>(field.offset()), field_type,
                            init->value.get(), ts);
            } catch (const std::exception &e) {
                throw std::runtime_error(fmt::format("DataDeclarationNode '{}': field '{}': {}",
                                                     m_name, field.name(), e.what()));
            }
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