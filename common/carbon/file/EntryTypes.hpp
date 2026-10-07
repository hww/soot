#pragma once

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/StringId.hpp"

namespace carbon {

    /// @brief Descriptor of a known entry type (DCEntry.m_typeId).
    /// @details Each entry in the DC file has an m_typeId that tells the loader
    ///          how to interpret its payload. This table lists the types known
    ///          to the inspector.
    struct EntryTypeInfo {
        sid64       type_sid;         ///< SID64 of the type name
        const char *name;             ///< human-readable name
        bool        is_known_payload; ///< true if the inspector knows how to parse the payload
    };

    /// @brief Table of entry types known to the inspector.
    /// @details Types with is_known_payload == false are printed as raw hex
    ///          by the inspector (their payload is opaque).
    inline constexpr EntryTypeInfo KNOWN_ENTRY_TYPES[] = {
        // Fully parsed:
        {SID("script-lambda"), "script-lambda", true},
        {SID("state-script"), "state-script", true},
        {SID("ss-type"), "ss-type", true},


        // Known by name but payload is opaque to the inspector:
        {SID("map"), "map", false},
        {SID("map-32"), "map-32", false},

        // Hypothetical data types (may or may not appear in real files):
        {SID("vector3"), "vector3", false},
        {SID("vector4"), "vector4", false},
        {SID("transform"), "transform", false},
        {SID("matrix"), "matrix", false},
        {SID("quaternion"), "quaternion", false},
        {SID("point"), "point", false},
        {SID("locator"), "locator", false},
        {SID("color"), "color", false},
        {SID("bound-frame"), "bound-frame", false},
    };

    /// @brief Look up an entry type by its SID64.
    /// @return pointer to the descriptor, or nullptr if the type is unknown.
    [[nodiscard]] inline const EntryTypeInfo *find_entry_type(sid64 type_sid) noexcept {
        for (const auto &info : KNOWN_ENTRY_TYPES) {
            if (info.type_sid == type_sid) { return &info; }
        }
        return nullptr;
    }

} // namespace carbon