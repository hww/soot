// dcinspect.cpp
//
// DC file inspector.
//
// Usage:
//   dcinspect <file.bin>                        # summary mode (default)
//   dcinspect --full <file.bin>                 # detailed mode
//   dcinspect --validate-only <file.bin>        # validation only
//   dcinspect --entries-only <file.bin>         # entries table only
//   dcinspect --decls-only <file.bin>           # declarations only
//   dcinspect --type-summary <file.bin>         # types overview only
//   dcinspect --dump-payload OFFSET <file.bin>  # hex dump at file offset
//
// Exit codes:
//   0 - ok
//   1 - unexpected error
//   2 - usage error
//   3 - cannot open file
//   4 - validation failed

#include "common/carbon/file/BinaryFile.hpp"
#include "common/carbon/file/BinaryFileInspector.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

    constexpr const char *TOOL_NAME = "dcinspect";
    constexpr const char *TOOL_VERSION = "v1.2";

    void print_usage() {
        std::cout <<
            R"(dcinspect — inspect a DC file (.bin)

Usage:
  dcinspect <file.bin>                        # summary mode
  dcinspect --full <file.bin>                 # detailed mode
  dcinspect --validate-only <file.bin>        # validation only
  dcinspect --entries-only <file.bin>         # entries table
  dcinspect --decls-only <file.bin>           # declarations only
  dcinspect --type-summary <file.bin>         # types overview
  dcinspect --dump-payload OFFSET <file.bin>  # hex dump at offset
  dcinspect --help
  dcinspect --version

OFFSET may be decimal (1234), hex (0x4D2), or octal (02322).
)";
    }

    /// @brief Parse an offset string: "1234", "0x4D2", or "02322".
    bool parse_offset(const std::string &s, u64 &out) {
        if (s.empty()) { return false; }
        char *end = nullptr;
        errno = 0;
        const unsigned long long v = std::strtoull(s.c_str(), &end, 0);
        if (errno != 0 || end == s.c_str() || *end != '\0') { return false; }
        out = static_cast<u64>(v);
        return true;
    }

    /// @brief Dump raw bytes at a file offset (hex + ASCII).
    void dump_hex(carbon::BinaryFile &file, u64 offset, u32 max_bytes) {
        if (offset >= file.m_size) {
            std::cerr << "error: offset 0x" << std::hex << offset << " >= file size 0x"
                      << file.m_size << "\n";
            return;
        }

        const auto *base = reinterpret_cast<const u8 *>(file.m_bytes.get());
        const u64   available = file.m_size - offset;
        const u32   n = static_cast<u32>(std::min<u64>(available, max_bytes));

        constexpr u32 BYTES_PER_LINE = 16;
        for (u32 i = 0; i < n; i += BYTES_PER_LINE) {
            std::printf("  %04llX  ", static_cast<unsigned long long>(offset + i));

            for (u32 j = 0; j < BYTES_PER_LINE; ++j) {
                if (i + j < n) {
                    std::printf("%02X ", base[offset + i + j]);
                } else {
                    std::printf("   ");
                }
                if (j == 7) { std::printf(" "); }
            }

            std::printf(" |");
            for (u32 j = 0; j < BYTES_PER_LINE && i + j < n; ++j) {
                const u8 c = base[offset + i + j];
                std::printf("%c", (c >= 32 && c < 127) ? static_cast<char>(c) : '.');
            }
            std::printf("|\n");
        }
    }

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 2;
    }

    enum class Mode {
        Summary,
        Full,
        ValidateOnly,
        EntriesOnly,
        DeclsOnly,
        TypeSummary,
        DumpPayload,
    };

    Mode        mode = Mode::Summary;
    std::string path;
    u64         dump_offset = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--help" || a == "-h") {
            print_usage();
            return 0;
        }
        if (a == "--version") {
            std::cout << TOOL_NAME << " " << TOOL_VERSION << "\n";
            return 0;
        }
        if (a == "--full") {
            mode = Mode::Full;
            continue;
        }
        if (a == "--validate-only") {
            mode = Mode::ValidateOnly;
            continue;
        }
        if (a == "--entries-only") {
            mode = Mode::EntriesOnly;
            continue;
        }
        if (a == "--decls-only") {
            mode = Mode::DeclsOnly;
            continue;
        }
        if (a == "--type-summary") {
            mode = Mode::TypeSummary;
            continue;
        }
        if (a == "--dump-payload") {
            if (i + 1 >= argc) {
                std::cerr << "error: --dump-payload requires an OFFSET argument\n";
                return 2;
            }
            if (!parse_offset(argv[++i], dump_offset)) {
                std::cerr << "error: invalid offset '" << argv[i] << "'\n";
                return 2;
            }
            mode = Mode::DumpPayload;
            continue;
        }
        if (!a.empty() && a[0] == '-') {
            std::cerr << "error: unknown option '" << a << "'\n";
            return 2;
        }
        if (!path.empty()) {
            std::cerr << "error: more than one input file given\n";
            return 2;
        }
        path = a;
    }

    if (path.empty()) {
        std::cerr << "error: no input file\n";
        print_usage();
        return 2;
    }

    try {
        auto file_res = carbon::BinaryFile::from_path(path);
        if (!file_res) {
            std::cerr << "error: " << file_res.error() << "\n";
            return 3;
        }

        carbon::BinaryFile &file = *file_res;

        const auto violations = file.validate();

        if (mode == Mode::ValidateOnly) {
            if (violations.empty()) {
                std::cout << "OK: no structural violations\n";
                return 0;
            }
            std::cout << "validation failed:\n";
            for (const auto &v : violations) { std::cout << "  - " << v << "\n"; }
            return 4;
        }

        if (mode == Mode::DumpPayload) {
            dump_hex(file, dump_offset, 512);
            return 0;
        }

        if (!violations.empty()) {
            std::cout << "validation warnings:\n";
            for (const auto &v : violations) { std::cout << "  - " << v << "\n"; }
            std::cout << "\n";
        }

        const auto inspect_mode = (mode == Mode::Full)
                                      ? carbon::BinaryFileInspector::InspectMode::Full
                                      : carbon::BinaryFileInspector::InspectMode::Summary;

        carbon::BinaryFileInspector inspector(&file, 2, inspect_mode);

        switch (mode) {
        case Mode::EntriesOnly: inspector.inspect_entry_summary(); break;
        case Mode::DeclsOnly: inspector.inspect_declarations_only(); break;
        case Mode::TypeSummary:
            inspector.inspect_overview();
            inspector.inspect_type_summary();
            break;
        default: inspector.inspect(); break;
        }

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}