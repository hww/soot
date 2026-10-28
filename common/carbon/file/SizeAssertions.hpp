#pragma once

#include "CommonTypes.hpp"
#include "file/DCHeader.hpp"
#include "file/DCScript.hpp"
#include "file/ProgramBinaryElement.hpp"

#include <cstddef>
#include <cstdio>
#include <type_traits>

namespace carbon {
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
            {"RequiresList", sizeof(RequiresList)},
        };

        std::printf("%-24s %8s   %s\n", "struct", "size", "hex");
        std::printf("%-24s %8s   %s\n", "------------------------", "--------", "-------");
        for (const auto &e : entries) {
            std::printf("%-24s %8zu   0x%zX\n", e.name, e.size, e.size);
        }
    }

} // namespace carbon