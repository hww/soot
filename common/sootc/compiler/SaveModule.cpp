// sootc/compiler/SaveModule.cpp
//
// Serialization: turning a FileNode into a .bin/.h on disk, and
// saving the whole REPL session. These methods know about
// FileNode, BinaryFile, and TypeSystem; they do not touch the
// compilation pipeline or the REPL dispatch.

#include "sootc/compiler/Compiler.hpp"

#include "carbon/file/BinaryFileInspector.hpp"
#include "carbon/lib/StringIdManager.hpp"

#include "sootc/compiler/NodeBuilder.hpp"

#include "sootc/node/DataDeclarationNode.hpp"
#include "sootc/node/EnumDeclarationNode.hpp"
#include "sootc/node/FileNode.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/HeapDefineNode.hpp"
#include "sootc/node/SequenceNode.hpp"
#include "sootc/node/StateScriptNode.hpp"
#include "sootc/node/StaticDefineNode.hpp"
#include "sootc/node/TypeDeclarationNode.hpp"

#include "type_system/TypeSystem.hpp"

#include "util/FileUtil.hpp"
#include "util/Log.hpp"
#include "util/ProjectConfig.hpp"

#include "fmt/color.h"
#include "fmt/format.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>

namespace sootc {

    // ================================================================
    // Binary / header output
    // ================================================================

    void Compiler::print_listing(const BinaryFile &file) {
        carbon::BinaryFileInspector inspector(const_cast<BinaryFile *>(&file));
        inspector.inspect();
    }

    bool Compiler::save_binary(const BinaryFile &file, const std::filesystem::path &target_dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(target_dir, ec);
        if (ec) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot create dir {}: {}\n",
                       target_dir.string(), ec.message());
            return false;
        }

        // ---- Strict mode: every declared method must have a body ----
        //
        // A REPL session can build a type and its methods in separate
        // inputs; build_ss_type must not fail on intermediate states.
        // But once the user asks to save the binary, every declared
        // method that is not a builtin must have a ScriptLambda.
        {
            const DCEntry *table = file.entries();
            const u32      n = file.entry_count();
            for (u32 i = 0; i < n; ++i) {
                if (BinaryFile::entry_kind(table[i]) != BinaryFile::EntryKind::SsType) continue;
                const SsType *st = file.entry_as_ss_type(table[i]);
                if (!st || !st->m_pMethods) continue;

                for (u32 id = 0; id < st->m_numMethods; ++id) {
                    const SsMethod &sm = static_cast<const SsMethod *>(st->m_pMethods)[id];
                    if (sm.m_pLambda != nullptr) continue;
                    if (sm.m_name == 0) continue; // empty slot

                    const std::string type_name = file.resolve_sid(st->m_name);
                    const std::string method_name = file.resolve_sid(sm.m_name);

                    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                               "; ERROR: method '{}' (id {}) of type '{}' has no defmethod\n",
                               method_name, id, type_name);
                    return false;
                }
            }
        }

        // Preserve the module structure under target_dir:
        // "lib/utils.sc" -> "<target_dir>/lib/utils.bin".
        fs::path rel = file.m_path;
        if (rel.is_absolute()) { rel = rel.lexically_relative(m_config.common.project_root); }
        rel.replace_extension(".bin");
        fs::path out = target_dir / rel;
        fs::create_directories(out.parent_path());

        if (!const_cast<BinaryFile &>(file).save(out)) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot save {}\n", out.string());
            return false;
        }

        fmt::print(fg(fmt::color::green), "; saved {}\n", out.string());
        return true;
    }

    bool Compiler::save_listing(const BinaryFile &file, const std::filesystem::path &target_dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(target_dir, ec);

        fs::path      out = target_dir / "out.lst";
        std::ofstream ofs(out);
        if (!ofs) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot open {}\n", out.string());
            return false;
        }

        // TODO: redirect BinaryFileInspector output here when its API
        // allows writing to a stream.
        ofs << "; SOOT listing\n";
        ofs << "; (not yet wired to BinaryFile API)\n";
        (void)file;

        fmt::print(fg(fmt::color::green), "; saved {}\n", out.string());
        return true;
    }

    bool Compiler::save_header(const BinaryFile &file, const std::filesystem::path &target_dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::create_directories(target_dir, ec);

        // Collect type names that actually have an SsType entry in the
        // binary. This filters out all builtin VM types (structure, basic,
        // symbol, type, string, function, ...) and only emits user types.
        std::vector<std::string> type_names;
        const DCEntry           *table = file.entries();
        const u32                n = file.entry_count();
        if (table) {
            for (u32 i = 0; i < n; ++i) {
                if (BinaryFile::entry_kind(table[i]) != BinaryFile::EntryKind::SsType) continue;
                const SsType *st = file.entry_as_ss_type(table[i]);
                if (!st) continue;
                const std::string name = file.resolve_sid(st->m_name);
                if (!name.empty()) { type_names.push_back(name); }
            }
        }

        if (type_names.empty()) { return true; } // nothing to emit

        std::string header = m_ts.generate_c_header(type_names);

        fs::path rel = file.m_path;
        if (rel.is_absolute()) { rel = rel.lexically_relative(m_config.common.project_root); }
        rel.replace_extension(".h");
        fs::path out = target_dir / rel;
        fs::create_directories(out.parent_path());

        std::ofstream ofs(out);
        if (!ofs) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot open {} for writing\n",
                       out.string());
            return false;
        }
        ofs << header;

        fmt::print(fg(fmt::color::green), "; saved {}\n", out.string());
        return true;
    }

    // ================================================================
    // Session
    // ================================================================

    bool Compiler::save_session(const std::string &name, const std::filesystem::path &dir) {
        namespace fs = std::filesystem;

        fs::path out_dir = dir;
        if (out_dir.is_relative()) {
            out_dir = common::get_path(common::PathType::PROJECT) / out_dir;
        }

        std::error_code ec;
        fs::create_directories(out_dir, ec);
        if (ec) {
            fmt::print(fg(fmt::color::crimson), "; ERROR: cannot create dir {}: {}\n",
                       out_dir.string(), ec.message());
            return false;
        }

        if (m_session_forms.empty()) {
            fmt::print(fg(fmt::color::yellow), "; WARN: session is empty, nothing to save\n");
            return false;
        }

        // save_session builds a throwaway FileNode from m_session_forms.
        // NodeBuilder::build_define binds every (define ...) into
        // m_global_env; without a rollback, those bindings point at
        // Nodes owned by the throwaway FileNode and dangle as soon as
        // it goes out of scope. Snapshot m_global_env before building
        // and roll it back on every exit path.
        GlobalNode::Snapshot global_snapshot = m_global_env->take_snapshot();

        try {
            NodeBuilder builder(m_ts, this);
            auto        file_node = std::make_unique<FileNode>(name);

            auto top_level_body = std::make_unique<SequenceNode>();
            auto top_level = std::make_unique<FunctionNode>("top-level");
            bool top_level_used = false;

            lg::debug("save_session: {} forms", m_session_forms.size());
            for (auto &f : m_session_forms) { lg::debug("save_session:   form: {}", f.print()); }

            for (auto &form : m_session_forms) {
                auto node = builder.build(form, file_node.get());
                if (!node) continue;

                if (auto *sd = dynamic_cast<StaticDefineNode *>(node.get())) {
                    file_node->add_child(sd->take_decl());
                    for (auto &store : sd->take_stores()) {
                        top_level_body->add(std::move(store));
                        top_level_used = true;
                    }
                } else if (auto *hd = dynamic_cast<HeapDefineNode *>(node.get())) {
                    file_node->add_child(hd->take_decl());
                    top_level_body->add(hd->take_store());
                    top_level_used = true;
                } else if (dynamic_cast<FunctionNode *>(node.get()) != nullptr ||
                           dynamic_cast<TypeDeclarationNode *>(node.get()) != nullptr ||
                           dynamic_cast<EnumDeclarationNode *>(node.get()) != nullptr ||
                           dynamic_cast<DataDeclarationNode *>(node.get()) != nullptr ||
                           dynamic_cast<StateScriptNode *>(node.get()) != nullptr) {
                    file_node->add_child(std::move(node));
                } else if (auto *expr = dynamic_cast<ExpressionNode *>(node.get())) {
                    if (auto *seq = dynamic_cast<SequenceNode *>(expr); seq && seq->size() == 0) {
                        // Empty sequence at top level: no code, drop it.
                    } else {
                        node.release();
                        top_level_body->add(std::unique_ptr<ExpressionNode>(expr));
                        top_level_used = true;
                    }
                }
            }

            if (top_level_used) {
                top_level->set_body(std::move(top_level_body));
                file_node->add_child(std::move(top_level));
            }

            // Compile state-script handler bodies.
            {
                std::vector<StateScriptNode *> state_scripts;
                for (auto &child : file_node->children()) {
                    if (auto *ss = dynamic_cast<StateScriptNode *>(child.get())) {
                        state_scripts.push_back(ss);
                    }
                }
                for (auto *ss : state_scripts) {
                    compile_state_script_bodies(ss, file_node.get(), builder);
                }
            }

            // ---- Strict mode: every method declared in a type must be
            //      implemented by a defmethod in the same FileNode. ----
            if (!check_type_methods_implemented(file_node.get())) {
                m_global_env->rollback(std::move(global_snapshot));
                return false;
            }

            // Strip self-dependency.
            //
            // build_require already skips a require whose stem matches
            // file->name(). But the session's identity is the `name`
            // argument of save_session, which may differ from any
            // FileNode::name() seen during the REPL (those are
            // "<repl-N>"). So the user can type (require "bar.sc") in a
            // session they then save as "bar", and the resulting
            // bar.bin would require "bar" â€” a self-cycle that cannot
            // be loaded. Remove it defensively here.
            {
                const std::string self = fs::path(name).stem().string();
                if (!self.empty()) {
                    const auto &reqs = file_node->get_requires();
                    if (std::find(reqs.begin(), reqs.end(), self) != reqs.end()) {
                        file_node->remove_require(self);
                    }
                }
            }

            DataSegment strings_table;
            auto        element = file_node->generate(strings_table);
            if (element.m_rawData.empty()) {
                fmt::print(fg(fmt::color::yellow), "; WARN: session produced no binary output\n");
                m_global_env->rollback(std::move(global_snapshot));
                return false;
            }

            auto bytes = make_aligned_buffer(element.m_rawData.size());
            std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());

            auto binary_result =
                BinaryFile::from_buffer(name, std::move(bytes), element.m_rawData.size());
            if (!binary_result) {
                fmt::print(fg(fmt::color::crimson), "; ERROR: {}\n", binary_result.error());
                m_global_env->rollback(std::move(global_snapshot));
                return false;
            }

            auto binary = std::make_unique<BinaryFile>(std::move(binary_result.value()));
            for (const auto &ds : file_node->data_structs()) {
                binary->m_dataStructs.push_back(ds);
            }

            if (m_config.compiler.print_listing) { print_listing(*binary); }

            bool ok = save_binary(*binary, out_dir);
            if (ok) ok = save_header(*binary, out_dir);

            save_sidbase();
            m_global_env->rollback(std::move(global_snapshot));
            return ok;
        } catch (const std::exception &e) {
            m_global_env->rollback(std::move(global_snapshot));
            fmt::print(fg(fmt::color::crimson), "; EXCEPTION during save_session: {}\n", e.what());
            return false;
        }
    }

    // ================================================================
    // Module / recursive module
    // ================================================================

    bool Compiler::save_module(const std::filesystem::path &source_file,
                               const std::filesystem::path &out_dir) {
        namespace fs = std::filesystem;

        auto ref_opt = resolve_module(source_file.string());
        if (!ref_opt) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                       "; ERROR: save_module: '{}' not found in project\n", source_file.string());
            return false;
        }
        const ModuleRef &ref = *ref_opt;

        FileNode                   *file_node = find_file_node(ref.id);
        std::unique_ptr<BinaryFile> binary_from_ast;

        if (file_node) {
            lg::info("save_module: reusing AST for '{}'", ref.id.string());

            // Strict mode: same check as save_session. Without it,
            // save_module could write a .bin whose type declares
            // methods that have no body, and the error would only
            // surface at load time.
            if (!check_type_methods_implemented(file_node)) { return false; }

            DataSegment strings_table;
            auto        element = file_node->generate(strings_table);
            if (element.m_rawData.empty()) {
                fmt::print(fg(fmt::color::yellow), "; WARN: save_module: '{}' produced no binary\n",
                           ref.id.string());
                return true;
            }

            auto bytes = make_aligned_buffer(element.m_rawData.size());
            std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());

            auto binary_result = BinaryFile::from_buffer(ref.id.string(), std::move(bytes),
                                                         element.m_rawData.size());
            if (!binary_result) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: save_module: cannot build binary: {}\n",
                           binary_result.error());
                return false;
            }

            binary_from_ast = std::make_unique<BinaryFile>(std::move(binary_result.value()));
            for (const auto &ds : file_node->data_structs()) {
                binary_from_ast->m_dataStructs.push_back(ds);
            }
        }

        fs::path target = out_dir;
        if (target.is_relative()) { target = m_config.common.project_root / target; }

        if (binary_from_ast) {
            return save_binary(*binary_from_ast, target) && save_header(*binary_from_ast, target);
        }

        auto result = compile_file(ref.resolved);
        if (!result) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                       "; ERROR: save_module: compile failed: {}\n", result.error());
            return false;
        }
        if (!*result) {
            fmt::print(fg(fmt::color::yellow), "; WARN: save_module: '{}' produced no binary\n",
                       ref.id.string());
            return true;
        }

        return save_binary(**result, target) && save_header(**result, target);
    }

    bool Compiler::save_recursive_impl(const ModuleRef &ref, const std::filesystem::path &out_dir,
                                       std::set<ModuleId> &visited, std::set<ModuleId> &active) {
        if (visited.count(ref.id)) return true;
        if (active.count(ref.id)) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                       "; ERROR: cyclic require: '{}'\n", ref.id.string());
            return false;
        }
        active.insert(ref.id);

        FileNode *file_node = find_file_node(ref.id);
        if (!file_node) {
            auto result = compile_file(ref.resolved);
            if (!result || !*result) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: save_module_recursive: cannot compile '{}'\n",
                           ref.id.string());
                return false;
            }
            file_node = find_file_node(ref.id);
            if (!file_node) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: save_module_recursive: AST not registered for '{}'\n",
                           ref.id.string());
                return false;
            }
        }

        for (const auto &dep : file_node->get_requires()) {
            if (dep == module_id_without_extension(ref.id)) continue;

            auto dep_ref_opt = resolve_require(dep + ".sc", ref.id);
            if (!dep_ref_opt) { dep_ref_opt = resolve_module(dep); }
            if (!dep_ref_opt) {
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: save_module_recursive: dependency '{}' of '{}' "
                           "not found in project\n",
                           dep, ref.id.string());
                return false;
            }
            if (!save_recursive_impl(*dep_ref_opt, out_dir, visited, active)) { return false; }
        }

        active.erase(ref.id);
        visited.insert(ref.id);

        return save_module(ref.resolved, out_dir);
    }

    bool Compiler::save_module_recursive(const std::filesystem::path &source_file,
                                         const std::filesystem::path &out_dir) {
        auto ref_opt = resolve_module(source_file.string());
        if (!ref_opt) {
            fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                       "; ERROR: save_module_recursive: '{}' not found in project\n",
                       source_file.string());
            return false;
        }
        std::set<ModuleId> visited;
        std::set<ModuleId> active;
        return save_recursive_impl(*ref_opt, out_dir, visited, active);
    }

    // ================================================================
    // SIDBase
    // ================================================================

    void Compiler::save_sidbase() {
        if (m_config.common.sidbase.empty()) {
            return; // no path configured, nothing to save
        }

        const std::string path = m_config.common.sidbase.string();
        const std::size_t count = carbon::StringIdManager::instance().size();
        const bool        saved = carbon::StringIdManager::instance().save_dconstruct_sidbase(path);

        if (saved) {
            lg::info("SIDBase: saved {} entries to '{}'", count, path);
        } else {
            lg::warn("SIDBase: failed to save '{}'", path);
        }
    }

    // ================================================================
    // Strict mode helper
    // ================================================================

    bool Compiler::check_type_methods_implemented(const FileNode *file_node) const {
        if (!file_node) return true;

        std::set<std::string> fn_names;
        for (auto &child : file_node->children()) {
            if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
                fn_names.insert(fn->name());
            }
        }

        for (auto &child : file_node->children()) {
            auto *td = dynamic_cast<TypeDeclarationNode *>(child.get());
            if (!td) continue;

            const std::string &type_name = td->type().base_type();
            Type              *type = m_ts.lookup_type_no_throw(type_name);
            if (!type) continue;

            for (const auto &m : type->get_methods_defined_for_type()) {
                const std::string full_name = type_name + "-" + m.name;
                if (fn_names.count(full_name)) continue;
                fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                           "; ERROR: method '{}' of type '{}' is declared "
                           "but has no defmethod\n",
                           m.name, type_name);
                return false;
            }

            if (const auto *ni = type->get_new_method_defined_for_type()) {
                const std::string full_name = type_name + "-" + ni->name;
                if (!fn_names.count(full_name)) {
                    fmt::print(fg(fmt::color::crimson) | fmt::emphasis::bold,
                               "; ERROR: method 'new' of type '{}' is declared "
                               "but has no defmethod\n",
                               type_name);
                    return false;
                }
            }
        }
        return true;
    }

} // namespace sootc