// sootc/compiler/ModuleRef.hpp
#pragma once

#include <filesystem>
#include <string>

namespace sootc {

    /// Logical module identifier: path relative to ProjectRoot.
    /// Examples: "lib/common/utils.sc", "src/main.sc".
    /// Stable across project moves, machine changes, and checkouts.
    using ModuleId = std::filesystem::path;

    /// Physical address: absolute canonical path to the file on disk.
    /// Used only for I/O: reading, mtime checks, symlink resolution.
    using ResolvedPath = std::filesystem::path;

    /// A module, identified logically and resolved physically.
    struct ModuleRef {
        ModuleId     id;       ///< e.g. "lib/common/utils.sc"
        ResolvedPath resolved; ///< e.g. "/work/demo/lib/common/utils.sc"
    };

    /// Strip the .sc extension from a ModuleId: "lib/utils.sc" -> "lib/utils".
    /// Used when writing the "requires" entry in a .bin: the .bin format
    /// stores logical names without extensions, so it does not depend on
    /// the source file's extension.
    inline std::string module_id_without_extension(const ModuleId &id) {
        auto p = id;
        p.replace_extension();
        return p.generic_string();
    }

} // namespace sootc