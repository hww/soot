#include "TypeDeclarationNode.hpp"

#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/StringIdManager.hpp"
#include "common/type_system/Type.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/util/Log.hpp"

#include <vector>

namespace sootc {

    /// @brief Emit an SsType entry (plus its SsField[] payload) for this type.
    /// @details See the header comment in TypeDeclarationNode.hpp.
    ProgramBinaryElement TypeDeclarationNode::generate(GlobalState &state) {
        (void)state;

        TypeSystem        &ts = TypeSystem::instance();
        const std::string &type_name = m_type.base_type();

        Type *type = ts.lookup_type_no_throw(type_name);
        if (!type) {
            lg::error("TypeDeclarationNode: unknown type '{}'", type_name);
            return ProgramBinaryElement(0);
        }

        auto *st = dynamic_cast<StructureType *>(type);
        if (!st) {
            // Only structures are emitted for now. Enums / value-types / basic
            // types will need their own SsType variants; until then, return an
            // empty element so the type is simply skipped.
            lg::info("TypeDeclarationNode: '{}' is not a structure; no SsType emitted", type_name);
            return ProgramBinaryElement(0);
        }

        // ------------------------------------------------------------------
        // 1. Build the SsField[] payload.
        // ------------------------------------------------------------------
        const auto &fields = st->fields();
        const u32   num_fields = static_cast<u32>(fields.size());

        std::vector<SsField> ss_fields(num_fields);
        for (u32 i = 0; i < num_fields; ++i) {
            const Field &f = fields[i];
            SsField     &sf = ss_fields[i];

            sf.m_name = StringId(f.name()).value;
            sf.m_type = StringId(f.type().base_type()).value;
            sf.m_offset = static_cast<u32>(f.offset());
            sf.m_size = static_cast<u32>(ts.get_size_in_type(f));
            sf.m_flags = 0;
            if (f.is_inline()) sf.m_flags |= 0x1;
            if (f.is_dynamic()) sf.m_flags |= 0x2;
            if (f.is_array()) sf.m_flags |= 0x4;
            sf.m_count = static_cast<u32>(f.array_size());
        }

        // ------------------------------------------------------------------
        // 2. Build the SsType header.
        // ------------------------------------------------------------------
        // m_pFields points just past the SsType header, i.e. at offset
        // sizeof(SsType) inside this element's payload. The relocation table
        // (below) marks this slot as relocatable, so FileNode::make_binary
        // will add the element's final file offset to it.
        SsType ss_type{};
        ss_type.m_name = StringId(type_name).value;
        ss_type.m_parent = StringId(type->get_parent()).value;
        ss_type.m_size = static_cast<u32>(st->get_size_in_memory());
        ss_type.m_align = static_cast<u32>(st->get_in_memory_alignment());
        ss_type.m_numFields = num_fields;
        ss_type.m_numMethods = 0;
        ss_type.m_pFields = reinterpret_cast<SsField *>(sizeof(SsType));
        ss_type.m_pMethods = nullptr;
        ss_type.m_flags = 0x2; // bit 1: is_structure
        ss_type.m_reserved = 0;

        // ------------------------------------------------------------------
        // 3. Assemble the ProgramBinaryElement.
        // ------------------------------------------------------------------
        ProgramBinaryElement element(sizeof(SsType) + num_fields * sizeof(SsField));

        element.m_entry = {
            .m_nameID = StringId(type_name).value, .m_typeId = SS_TYPE_SID, .m_entryPtr = nullptr};

        // SsType is 8 slots (0x40 bytes). Only slots 4 and 5 — m_pFields and
        // m_pMethods — are relocated pointers.
        element.push_bytes(ss_type, {0, 0, 0, 0, 1, 1, 0, 0});

        // SsField is 4 slots (0x20 bytes). No pointers inside.
        for (const auto &sf : ss_fields) { element.push_bytes(sf, {0, 0, 0, 0}); }

        lg::info("TypeDeclarationNode '{}': emitted SsType entry ({} fields, {} bytes)", type_name,
                 num_fields, element.m_rawData.size());

        return element;
    }

} // namespace sootc