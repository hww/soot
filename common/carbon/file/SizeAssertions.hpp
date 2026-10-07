#pragma once

#include "CommonTypes.hpp"
#include "file/DCHeader.hpp"
#include "file/DCScript.hpp"
#include "file/ProgramBinaryElement.hpp"

#include <cstddef>
#include <cstdio>
#include <type_traits>

namespace carbon {

    /// Compile-time size checks. If any of these fails, the build stops.
    /// Uncomment only the ones you want to enforce; comment out the ones
    /// that differ in your format (e.g. ScriptLambda is 0x58 here, not 0x50).
    namespace size_checks {

        // DCHeader.hpp
        static_assert(sizeof(DCEntry) == 0x18, "DCEntry must be 0x18 bytes");
        static_assert(sizeof(DC_Header) == 0x20, "DC_Header must be 0x20 bytes");

        // DCScript.hpp
        static_assert(sizeof(SsDeclarationList) == 0x10, "SsDeclarationList must be 0x10 bytes");
        static_assert(sizeof(SsDeclaration) == 0x30, "SsDeclaration must be 0x30 bytes");
        static_assert(sizeof(SymbolArray) == 0x10, "SymbolArray must be 0x10 bytes");
        static_assert(sizeof(SsOptions) == 0x50, "SsOptions must be 0x50 bytes");
        static_assert(sizeof(StateScript) == 0x50, "StateScript must be 0x50 bytes");
        static_assert(sizeof(SsState) == 0x18, "SsState must be 0x18 bytes");
        static_assert(sizeof(SsOnBlock) == 0x50, "SsOnBlock must be 0x50 bytes");
        static_assert(sizeof(SsTrackGroup) == 0x38, "SsTrackGroup must be 0x38 bytes");
        static_assert(sizeof(SsTrack) == 0x18, "SsTrack must be 0x18 bytes");
        static_assert(sizeof(SsLambda) == 0x10, "SsLambda must be 0x10 bytes");

        // SsType — self-describing type information stored in the .bin file.
        static_assert(sizeof(SsField) == 0x20, "SsField must be 0x20 bytes");
        static_assert(sizeof(SsType) == 0x40, "SsType must be 0x40 bytes");

        // ScriptLambda is 0x58 in the carbon format (two extra fields not present
        // in the icemesh reference layout, where it is 0x50).
        static_assert(sizeof(ScriptLambda) == 0x58, "ScriptLambda must be 0x58 bytes");

    } // namespace size_checks

    /// Runtime helper: prints a table of all struct sizes to stdout.
    /// Useful from a test or from main() to sanity-check after editing headers.
    inline void print_all_struct_sizes() {
        struct Entry {
            const char *name;
            size_t      size;
        };
        const Entry entries[] = {
            {"DCEntry", sizeof(DCEntry)},
            {"DC_Header", sizeof(DC_Header)},
            {"SsDeclarationList", sizeof(SsDeclarationList)},
            {"SsDeclaration", sizeof(SsDeclaration)},
            {"SymbolArray", sizeof(SymbolArray)},
            {"SsOptions", sizeof(SsOptions)},
            {"StateScript", sizeof(StateScript)},
            {"SsState", sizeof(SsState)},
            {"SsOnBlock", sizeof(SsOnBlock)},
            {"SsTrackGroup", sizeof(SsTrackGroup)},
            {"SsTrack", sizeof(SsTrack)},
            {"SsLambda", sizeof(SsLambda)},
            {"ScriptLambda", sizeof(ScriptLambda)},
            {"SsField", sizeof(SsField)},
            {"SsType", sizeof(SsType)},
            {"ProgramBinaryElement", sizeof(ProgramBinaryElement)},
        };

        std::printf("%-24s %8s   %s\n", "struct", "size", "hex");
        std::printf("%-24s %8s   %s\n", "------------------------", "--------", "-------");
        for (const auto &e : entries) {
            std::printf("%-24s %8zu   0x%zX\n", e.name, e.size, e.size);
        }
    }

} // namespace carbon