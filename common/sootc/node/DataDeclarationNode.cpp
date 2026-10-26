#include "DataDeclarationNode.hpp"
#include "carbon/lib/StringId.hpp"
#include "carbon/lib/StringIdManager.hpp"
#include "sootc/libs/DataSegment.hpp"
#include "sootc/node/ConstFold.hpp"
#include "sootc/node/ConstNode.hpp"
#include "sootc/node/VariableNode.hpp"
#include "sootc/node/FileNode.hpp"
#include "type_system/Type.hpp"
#include "type_system/TypeSpec.hpp"
#include "type_system/TypeSystem.hpp"
#include "util/Log.hpp"

#include "fmt/format.h"

#include <atomic>
#include <cstring>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

// ============================================================================
// Helpers
// ============================================================================
namespace {

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

        // Convenience alias for the list of anonymous child entries that
        // generate_static_element and write_value produce while walking
        // a (new 'static ...) tree.
        using AnonChildren = std::vector<std::pair<std::string, ProgramBinaryElement>>;

        // Forward declarations. write_value is recursive and calls
        // generate_static_element for anonymous static structures, while
        // generate_static_element calls write_value for each field.
        // Both signatures must agree with their definitions below.

        void write_value(std::vector<std::byte> &payload, size_t base_off, Type *type,
                         const ExpressionNode *init, TypeSystem &ts, DataSegment &segment,
                         std::vector<ProgramBinaryElement::StructPointerSlot> &deferred,
                         AnonChildren &anon_children, const std::string &owner_name,
                         const std::string &field_name, bool is_inline);

        ProgramBinaryElement generate_static_element(const NewNode *node, TypeSystem &ts,
                                                     DataSegment       &segment,
                                                     const std::string &owner_name);

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
                const sid64 v = StringId(cn->string_value()).value;
                std::memcpy(payload.data() + off, &v, sizeof(sid64));
                return;
            }
            if (type_name == "bool" || type_name == "boolean") {
                const u8 v = cn->int_value() ? 1 : 0;
                payload[off] = static_cast<std::byte>(v);
                return;
            }

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

        const NewNode::FieldInit *find_field_init(const NewNode *node, const std::string &name) {
            if (!node) { return nullptr; }
            for (const auto &fi : node->fields()) {
                if (fi.name == name) { return &fi; }
            }
            return nullptr;
        }

        // Recursively write a compile-time value at base_off.
        //
        // Supported cases:
        //   1. builtin scalar          - fold + memcpy.
        //   2. inline structure        - recurse.
        //   3. pointer to anonymous    - generate an anon entry and record
        //      static structure          a deferred slot that points at it.
        //
        // Inline arrays and non-static pointers are not yet supported and
        // are left at zero in the payload; build_define creates a
        // StoreFieldNode for them.
        void write_value(std::vector<std::byte> &payload, size_t base_off, Type *type,
                         const ExpressionNode *init, TypeSystem &ts, DataSegment &segment,
                         std::vector<ProgramBinaryElement::StructPointerSlot> &deferred,
                         AnonChildren &anon_children, const std::string &owner_name,
                         const std::string &field_name, bool is_inline) {
            const std::string &type_name = type->get_name();

            // ---- 0. Pointer to a structure ----
            //
            // A reference-typed field that is NOT inline holds a pointer,
            // not an embedded value. We do not recurse into the type's
            // fields here: writing subfields at (base_off + sub.offset)
            // would corrupt the parent's payload.
            if (!is_inline && type->is_reference()) {
                // ---- 0a. Anonymous static structure ----
                //
                // The initializer is a (new 'static X ...) form. We
                // synthesize a name from the owner and the field, generate
                // the child element, and record a deferred slot that will
                // be resolved to the child's file offset in make_binary.
                if (auto *new_init = dynamic_cast<const NewNode *>(init)) {
                    if (new_init->is_static()) {
                        // Resolve the owner FileNode through the parent
                        // chain. The init node is attached to the
                        // DataDeclarationNode, which is attached to the
                        // FileNode. Node::file() walks that chain.
                        //
                        // If the chain is broken (e.g. a detached
                        // expression being inspected in a test), fall
                        // back to the raw base name — uniqueness is a
                        // convenience, not a correctness requirement.
                        FileNode         *owner_file = const_cast<ExpressionNode *>(init)->file();
                        const std::string base = owner_name + "-" + field_name;
                        const std::string anon_name =
                            owner_file ? owner_file->make_unique_anon_name(base) : base;

                        ProgramBinaryElement anon_el =
                            generate_static_element(new_init, ts, segment, anon_name);

                        anon_children.push_back({anon_name, std::move(anon_el)});

                        ProgramBinaryElement::StructPointerSlot slot{};
                        slot.kind = ProgramBinaryElement::StructPointerSlot::Kind::NamedStatic;
                        slot.slot_offset = base_off;
                        slot.target_name = anon_name;
                        deferred.push_back(std::move(slot));
                        return;
                    }
                }

                // ---- 0b. Named static structure in the same file ----
                //
                // The initializer is a VariableNode naming another
                // top-level declaration. We do not know its file offset
                // here; we record the name and let make_binary resolve it
                // after layout.
                if (auto *var_init = dynamic_cast<const VariableNode *>(init)) {
                    ProgramBinaryElement::StructPointerSlot slot{};
                    slot.kind = ProgramBinaryElement::StructPointerSlot::Kind::NamedStatic;
                    slot.slot_offset = base_off;
                    slot.target_name = var_init->name();
                    deferred.push_back(std::move(slot));
                    return;
                }

                // Not resolvable at compile time. Leave the slot at zero;
                // build_define creates a StoreFieldNode for this field and
                // the real value is written at runtime.
                return;
            }

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
            //
            // Reached only when the field is inline (is_inline == true)
            // and the type is a structure. The initializer must be a
            // static NewNode; we recurse for each of its subfields,
            // shifting each subfield's offset by base_off.
            if (auto *st = dynamic_cast<StructureType *>(type)) {
                auto *new_node = dynamic_cast<const NewNode *>(init);
                if (!new_node || !new_node->is_static()) {
                    throw std::runtime_error(
                        fmt::format("inline field of type '{}' needs a compile-time "
                                    "(new 'static '{} ...) initializer",
                                    type_name, type_name));
                }

                for (const auto &sub : st->fields()) {
                    const NewNode::FieldInit *sub_init = find_field_init(new_node, sub.name());
                    if (!sub_init || !sub_init->value) { continue; }

                    Type *sub_type = ts.lookup_type_no_throw(sub.type().base_type());
                    if (!sub_type) {
                        throw std::runtime_error(
                            fmt::format("unknown subfield type '{}' for field '{}.{}'",
                                        sub.type().print(), type_name, sub.name()));
                    }

                    write_value(payload, base_off + static_cast<size_t>(sub.offset()), sub_type,
                                sub_init->value.get(), ts, segment, deferred, anon_children,
                                owner_name, sub.name(),
                                sub.is_inline()); // recurse with the subfield's own inline flag
                }
                return;
            }

            // ---- Unsupported ----
            // Not resolvable at compile time. Leave the slot at zero;
            // build_define creates a StoreFieldNode for this field and
            // the real value is written at runtime.
            return;
        }

        // Generate a ProgramBinaryElement for a static NewNode.
        //
        // Produces the payload (and relocations) for a (new 'static X ...)
        // form. Does NOT set the entry name or type; the caller does.
        // Used both by DataDeclarationNode::generate for the top-level
        // define, and internally for anonymous static structures that are
        // referenced from a pointer field.
        //
        // Any anonymous children produced while walking this structure are
        // attached to the returned element's m_anonChildren. The caller
        // (write_value for a nested anon, or FileNode::collect_all for a
        // top-level declaration) is responsible for flattening them into
        // the entry list.
        ProgramBinaryElement generate_static_element(const NewNode *node, TypeSystem &ts,
                                                     DataSegment       &segment,
                                                     const std::string &owner_name) {
            if (!node) { throw std::runtime_error("generate_static_element: null node"); }

            const std::string &type_name = node->type().base_type();
            Type              *type = ts.lookup_type_no_throw(type_name);
            if (!type) {
                throw std::runtime_error(
                    fmt::format("generate_static_element: unknown type '{}'", type_name));
            }
            auto *st = dynamic_cast<StructureType *>(type);
            if (!st) {
                throw std::runtime_error(fmt::format(
                    "generate_static_element: type '{}' is not a structure", type_name));
            }

            const int size_bytes = st->get_size_in_memory();
            if (size_bytes <= 0) {
                throw std::runtime_error(
                    fmt::format("generate_static_element: type '{}' has invalid size {}", type_name,
                                size_bytes));
            }

            std::vector<std::byte> payload(static_cast<size_t>(size_bytes), std::byte{0});
            std::vector<ProgramBinaryElement::StructPointerSlot> deferred;

            // Anonymous children discovered while walking this structure's
            // fields. They are attached to the element below.
            AnonChildren local_children;

            for (const auto &field : st->fields()) {
                const NewNode::FieldInit *init = find_field_init(node, field.name());
                if (!init || !init->value) { continue; }

                Type *field_type = ts.lookup_type_no_throw(field.type().base_type());
                if (!field_type) {
                    lg::warn("generate_static_element '{}': field '{}' unknown type '{}'",
                             type_name, field.name(), field.type().print());
                    continue;
                }

                write_value(payload, static_cast<size_t>(field.offset()), field_type,
                            init->value.get(), ts, segment, deferred, local_children, owner_name,
                            field.name(), field.is_inline());
            }

            ProgramBinaryElement el(static_cast<u64>(payload.size()));

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
            el.m_structLayout = std::move(layout);

            el.push_blob(payload.data(), payload.size(), /*relocation_bit=*/0);

            el.m_structPointerSlots = std::move(deferred);
            el.m_anonChildren = std::move(local_children);
            return el;
        }

    } // namespace

    ProgramBinaryElement DataDeclarationNode::generate(DataSegment &state) {
        if (!m_instance) {
            lg::warn("DataDeclarationNode '{}' has no instance", m_name);
            return ProgramBinaryElement(0);
        }

        if (m_heap) {
            lg::debug("DataDeclarationNode '{}': heap, no payload", m_name);
            return ProgramBinaryElement(0);
        }

        if (!m_instance->is_static()) {
            lg::error("DataDeclarationNode '{}': only 'static' allocation is "
                      "valid at top level, got '{}'",
                      m_name, m_instance->allocation());
            return ProgramBinaryElement(0);
        }

        try {
            ProgramBinaryElement el =
                generate_static_element(m_instance.get(), TypeSystem::instance(), state, m_name);

            el.m_entry = {.m_nameID = StringId(m_name).value,
                          .m_typeId = StringId(m_instance->type().base_type()).value,
                          .m_entryPtr = nullptr};

            lg::info("DataDeclarationNode '{}': {} bytes, type '{}'", m_name,
                     el.m_rawData.size(), m_instance->type().base_type());
            return el;
        } catch (const std::exception &e) {
            throw std::runtime_error(fmt::format("DataDeclarationNode '{}': {}", m_name, e.what()));
        }
    }

} // namespace sootc