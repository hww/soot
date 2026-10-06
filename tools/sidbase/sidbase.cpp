// sidbase.cpp
//
// String-id database manager for the DC format.
//
// Commands:
//   build  — scan C/C++/H/HPP files and write a fresh sidbase.bin
//   add    — add entries from C/C++ files to an existing sidbase.bin
//   merge  — merge one or more sidbase.bin files into one
//   list   — print entries of a sidbase.bin (all / by wildcard)
//
// Example:
//   sidbase build  --out sidbase.bin --recursive src/
//   sidbase add    --base sidbase.bin --out sidbase.bin src/
//   sidbase merge  --out merged.bin --base a.bin --base b.bin
//   sidbase list   --base sidbase.bin --name 'print*'
//
// The output format is the canonical dconstruct binary sidbase:
//   u64 num_entries
//   SIDBaseEntry[num_entries] { u64 hash; u64 offset; }
//   char pool[]  (NUL-terminated strings)

#include "Args.hpp"

#include "common/CommonTypes.hpp"
#include "common/carbon/lib/SIDBase.hpp"
#include "common/carbon/lib/StringId.hpp"
#include "common/carbon/lib/StringIdManager.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

    // ===========================================================================
    // Constants
    // ===========================================================================

    constexpr const char *TOOL_NAME = "sidbase";
    constexpr const char *TOOL_VERSION = "v1.0";

    // ===========================================================================
    // C/C++ scanner
    // ===========================================================================

    // Matches SID("..."), SID32("..."), SID64("...") with optional whitespace
    // inside parentheses. Only quoted strings are matched; SID(x) is skipped.
    constexpr const char *SID_PATTERN =
        R"sid(\bSID(?:32|64)?[[:space:]]*\([[:space:]]*"([^"\r\n]*)"[[:space:]]*\))sid";

    const std::regex &sid_regex() {
        static const std::regex re(SID_PATTERN, std::regex::extended);
        return re;
    }

    bool is_source_file(const fs::path &p) {
        const std::string ext = p.extension().string();
        return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" || ext == ".h" ||
               ext == ".hh" || ext == ".hpp" || ext == ".hxx" || ext == ".inl" || ext == ".ipp";
    }

    void scan_text(const std::string &text, carbon::StringIdManager &mgr, bool verbose) {
        std::smatch match;
        auto        search_start = text.cbegin();
        while (std::regex_search(search_start, text.cend(), match, sid_regex())) {
            const std::string name = match[1].str();
            if (verbose) { std::cout << "  SID: \"" << name << "\"\n"; }
            mgr.register_string(name);
            search_start = match.suffix().first;
        }
    }

    void scan_file(const fs::path &path, carbon::StringIdManager &mgr, bool verbose) {
        std::ifstream in(path);
        if (!in.is_open()) {
            std::cerr << "warning: cannot open '" << path << "'\n";
            return;
        }
        if (verbose) { std::cout << "scanning " << path << "\n"; }
        std::stringstream buf;
        buf << in.rdbuf();
        scan_text(buf.str(), mgr, verbose);
    }

    void scan_path(const fs::path &path, bool recursive, carbon::StringIdManager &mgr,
                   bool verbose) {
        if (fs::is_regular_file(path)) {
            if (is_source_file(path)) { scan_file(path, mgr, verbose); }
            return;
        }
        if (!fs::is_directory(path)) {
            std::cerr << "warning: skipping non-regular path '" << path << "'\n";
            return;
        }
        if (recursive) {
            for (const auto &entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file() && is_source_file(entry.path())) {
                    scan_file(entry.path(), mgr, verbose);
                }
            }
        } else {
            for (const auto &entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file() && is_source_file(entry.path())) {
                    scan_file(entry.path(), mgr, verbose);
                }
            }
        }
    }

    // ===========================================================================
    // Wildcard matching (for `list --name PATTERN`)
    // ===========================================================================

    bool wildcard_match(const std::string &pattern, const std::string &text) {
        std::string re;
        re.reserve(pattern.size() * 2);
        for (char c : pattern) {
            if (c == '*') {
                re += ".*";
            } else if (c == '?') {
                re += ".";
            } else if (std::string(".^$|()[]{}*+?\\").find(c) != std::string::npos) {
                re += '\\';
                re += c;
            } else {
                re += c;
            }
        }
        try {
            const std::regex re_obj(re, std::regex::ECMAScript);
            return std::regex_search(text, re_obj);
        } catch (const std::regex_error &) { return pattern == text; }
    }

    // ===========================================================================
    // Commands
    // ===========================================================================

    int cmd_help() {
        std::cout <<
            R"(sidbase — string-id database manager for the DC format

Usage:
  sidbase build  --out FILE             [--recursive] [--verbose] PATH...
  sidbase add    --base FILE --out FILE [--recursive] [--verbose] PATH...
  sidbase merge  --out FILE --base FILE [--base FILE ...] [--verbose]
  sidbase list   --base FILE            [--name PATTERN] [--verbose]

Commands:
  build   Scan C/C++/H/HPP files (or folders) and write a fresh sidbase.bin.
  add     Like build, but starts from an existing sidbase.bin and appends new entries.
  merge   Merge one or more existing sidbase.bin files into a new one.
  list    Print entries of an existing sidbase.bin.
          --name PATTERN  wildcard filter (* and ? supported), e.g. --name 'print*'

Options:
  --out FILE         output sidbase.bin path
  --base FILE        input sidbase.bin (repeatable)
  --recursive        scan folders recursively
  --verbose          print every scanned file and every found SID
  --name PATTERN     wildcard filter for the `list` command
  --help             print this message

Examples:
  sidbase build --out sidbase.bin --recursive src/
  sidbase add   --base sidbase.bin --out sidbase.bin src/
  sidbase merge --out merged.bin --base a.bin --base b.bin
  sidbase list  --base sidbase.bin --name 'print*'
)";
        return 0;
    }

    int cmd_build(const sider::Args &args) {
        const fs::path out_path = args.require("out");
        const bool     recursive = args.has("recursive");
        const bool     verbose = args.has("verbose");
        const auto    &paths = args.positional();

        if (paths.empty()) {
            std::cerr << "error: build: no input paths\n";
            return 2;
        }

        auto &mgr = carbon::StringIdManager::instance();
        mgr.clear();

        for (const auto &p : paths) { scan_path(p, recursive, mgr, verbose); }

        std::cout << "collected " << mgr.size() << " string ids\n";
        if (!mgr.save_dconstruct_sidbase(out_path.string())) {
            std::cerr << "error: failed to write '" << out_path << "'\n";
            return 3;
        }
        std::cout << "wrote " << out_path << "\n";
        return 0;
    }

    int cmd_add(const sider::Args &args) {
        const fs::path base_path = args.require("base");
        const fs::path out_path = args.require("out");
        const bool     recursive = args.has("recursive");
        const bool     verbose = args.has("verbose");
        const auto    &paths = args.positional();

        if (paths.empty()) {
            std::cerr << "error: add: no input paths\n";
            return 2;
        }

        auto &mgr = carbon::StringIdManager::instance();
        if (!mgr.load_dconstruct_sidbase(base_path.string())) {
            std::cerr << "error: cannot load '" << base_path << "'\n";
            return 3;
        }
        const size_t before = mgr.size();

        for (const auto &p : paths) { scan_path(p, recursive, mgr, verbose); }

        std::cout << "added " << (mgr.size() - before) << " new string ids\n";
        if (!mgr.save_dconstruct_sidbase(out_path.string())) {
            std::cerr << "error: failed to write '" << out_path << "'\n";
            return 3;
        }
        std::cout << "wrote " << out_path << "\n";
        return 0;
    }

    int cmd_merge(const sider::Args &args) {
        const fs::path out_path = args.require("out");
        const bool     verbose = args.has("verbose");
        const auto    &bases = args.get_all("base");

        if (bases.empty()) {
            std::cerr << "error: merge: at least one --base is required\n";
            return 2;
        }

        auto &mgr = carbon::StringIdManager::instance();
        mgr.clear();

        for (const auto &base : bases) {
            if (!mgr.load_dconstruct_sidbase(base)) {
                std::cerr << "error: cannot load '" << base << "'\n";
                return 3;
            }
            if (verbose) { std::cout << "loaded " << base << " (" << mgr.size() << " total)\n"; }
        }

        if (!mgr.save_dconstruct_sidbase(out_path.string())) {
            std::cerr << "error: failed to write '" << out_path << "'\n";
            return 3;
        }
        std::cout << "merged " << mgr.size() << " entries into " << out_path << "\n";
        return 0;
    }

    int cmd_list(const sider::Args &args) {
        const fs::path    base_path = args.require("base");
        const bool        verbose = args.has("verbose");
        const std::string filter = args.get("name", "");

        auto &mgr = carbon::StringIdManager::instance();
        mgr.clear();
        if (!mgr.load_dconstruct_sidbase(base_path.string())) {
            std::cerr << "error: cannot load '" << base_path << "'\n";
            return 3;
        }

        // Snapshot and sort by hash for stable output.
        std::vector<std::pair<sid64, std::string>> entries;
        entries.reserve(mgr.size());
        for (const auto &[id, name] : mgr) { entries.emplace_back(id, name); }
        std::sort(entries.begin(), entries.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });

        size_t printed = 0;
        for (const auto &[hash, name] : entries) {
            if (!filter.empty() && !wildcard_match(filter, name)) { continue; }
            std::cout << fmt::format("{:016X} {}\n", hash, name);
            ++printed;
        }

        if (verbose) { std::cerr << "printed " << printed << " of " << mgr.size() << " entries\n"; }
        return 0;
    }

} // namespace

// ===========================================================================
// main
// ===========================================================================

int main(int argc, const char **argv) {
    if (argc < 2) {
        cmd_help();
        return 0;
    }

    const std::string command = argv[1];
    if (command == "--help" || command == "-h" || command == "help") { return cmd_help(); }
    if (command == "--version") {
        std::cout << TOOL_NAME << " " << TOOL_VERSION << "\n";
        return 0;
    }

    sider::Args args(argc, argv);

    try {
        if (command == "build") return cmd_build(args);
        else if (command == "add")
            return cmd_add(args);
        else if (command == "merge")
            return cmd_merge(args);
        else if (command == "list")
            return cmd_list(args);
        std::cerr << "error: unknown command '" << command << "'\n";
        return 2;
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}