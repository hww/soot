// FileNode.cpp
#include "sootc/node/FileNode.hpp"
#include "DataDeclarationNode.hpp"
#include "EnumDeclarationNode.hpp"
#include "TypeDeclarationNode.hpp"
#include "carbon/file/DCHeader.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "util/Log.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include "sootc/node/DataDeclarationNode.hpp"

#include <cassert>
#include <cstring>
#include <numeric>
#include <type_system/TypeSystem.hpp>
#include <unordered_map>

using namespace carbon;

namespace sootc {

    // ============================================================================
    // Constructor
    // ============================================================================
    FileNode::FileNode(const std::string &name) : Node(NodeType::FileNode), m_name(name) {}

    // ============================================================================
    // to_string
    // ============================================================================
    std::string FileNode::to_string() const {
        return "FileNode(name=" + m_name + ", symbols=" + std::to_string(m_symbols.size()) +
               ", requires=" + std::to_string(m_requires.size()) + ")";
    }

    // ============================================================================
    // generate - главный метод генерации бинарника (интерфейс Node)
    // ============================================================================
    ProgramBinaryElement FileNode::generate(DataSegment &strings_table) {
        auto entries = collect_all(strings_table);

        bool has_types = false;
        for (auto &child : m_children) {
            if (dynamic_cast<TypeDeclarationNode *>(child.get()) != nullptr) {
                has_types = true;
                break;
            }
        }

        if (entries.empty() && !has_types) {
            lg::info("FileNode::generate: nothing to emit ({} children, 0 emittable)",
                     m_children.size());
            return ProgramBinaryElement(0);
        }

        std::vector<DataStructEntry> data_structs;
        auto element = make_binary(std::move(entries), strings_table, data_structs);
        m_dataStructs = std::move(data_structs);

        // Advance the watermark. After this call, collect_all will start
        // from the new index, so previously emitted children are skipped.


        return element;
    }

    // ============================================================================
    // collect_all - собирает ProgramBinaryElement для всех функций
    // ============================================================================
    std::vector<ProgramBinaryElement> FileNode::collect_all(DataSegment &data_segment) {
        std::vector<ProgramBinaryElement> entries;

        for (auto &child : m_children) {
            if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
                fn->set_global_state(&data_segment);
                fn->emit_body();
                entries.push_back(fn->generate(data_segment));
                lg::debug("Function '{}': {} instructions, {} constants", fn->name(),
                          fn->instructions().size(), fn->constants().size());
            } else if (auto *decl = dynamic_cast<DataDeclarationNode *>(child.get())) {
                ProgramBinaryElement element = decl->generate(data_segment);
                const size_t         payload_size = element.m_rawData.size();
                if (payload_size > 0 || !element.m_anonChildren.empty()) {
                    // Children first, then the parent. Order in the entry
                    // table is irrelevant for name resolution, but a
                    // parent-after-children layout reads better in dumps.
                    flatten_anon(element, entries);
                    if (payload_size > 0) {
                        entries.push_back(std::move(element));
                        lg::debug("Data declaration '{}': {} bytes", decl->name(), payload_size);
                    }
                } else if (decl->is_heap()) {
                    lg::debug("Data declaration '{}': heap, no payload", decl->name());
                } else {
                    lg::warn("Data declaration '{}' produced empty element", decl->name());
                }
            }
            // StateScriptNode: no payload of its own. Handler bodies and
            // track steps were compiled into FunctionNode children by
            // Compiler::compile_state_script_bodies BEFORE generate().
            // The StateScript entry itself is emitted by make_binary
            // from the StateScriptNode's Type (see build_state_script).
        }

        return entries;
    }

    // ============================================================================
    // flatten_anon - recursively flatten m_anonChildren into the entry list
    // ============================================================================

    void FileNode::flatten_anon(ProgramBinaryElement &el, std::vector<ProgramBinaryElement> &out) {
        for (auto &[name, child] : el.m_anonChildren) {
            child.m_entry = {
                .m_nameID = StringId(name).value,
                .m_typeId =
                    StringId(child.m_structLayout ? child.m_structLayout->type_name : "global")
                        .value,
                .m_entryPtr = nullptr,
            };
            flatten_anon(child, out);
            out.push_back(std::move(child));
        }
        el.m_anonChildren.clear();
    }

    // ============================================================================
    // build_ss_type - emit one SsType element (header + SsField[] + SsMethod[])
    // ============================================================================
    ProgramBinaryElement
    FileNode::build_ss_type(const TypeDeclarationNode                  *type_decl,
                            const std::unordered_map<std::string, u64> &type_lambdas) {

        TypeSystem        &ts = TypeSystem::instance();
        const std::string &type_name = type_decl->type().base_type();

        Type *type = ts.lookup_type_no_throw(type_name);
        if (!type) {
            lg::error("build_ss_type: unknown type '{}'", type_name);
            return ProgramBinaryElement(0);
        }

        auto *st = dynamic_cast<StructureType *>(type);
        if (!st) {
            lg::info("build_ss_type: '{}' is not a structure; skipped", type_name);
            return ProgramBinaryElement(0);
        }

        // ------------------------------------------------------------------
        // 1. SsField[] — one entry per field.
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
        // 2. SsMethod[] — one entry per method, indexed by ID.
        // ------------------------------------------------------------------
        const int max_method_id = ts.get_type_method_count(type_name) - 1;
        const u32 num_methods = (max_method_id >= 0) ? static_cast<u32>(max_method_id + 1) : 0;

        std::vector<SsMethod> ss_methods(num_methods);
        for (u32 id = 0; id < num_methods; ++id) {
            MethodInfo info;
            if (!ts.try_lookup_method(type_name, static_cast<int>(id), &info)) {
                ss_methods[id] = {0, 0, nullptr};
                continue;
            }

            SsMethod &sm = ss_methods[id];
            sm.m_name = StringId(info.name).value;

            // Full name is "<type>-<method>" — this is the name under which
            // the method's ScriptLambda is registered in Globals. It is used
            // for runtime lookup and for introspection.
            const std::string full_name = fmt::format("{}-{}", type_name, info.name);
            sm.m_fullName = StringId(full_name).value;

            auto it = type_lambdas.find(info.name);
            if (it != type_lambdas.end()) {
                sm.m_pLambda = reinterpret_cast<ScriptLambda *>(it->second);
            } else {
                sm.m_pLambda = nullptr;
            }
        }

        // ------------------------------------------------------------------
        // 3. Assemble the ProgramBinaryElement.
        // ------------------------------------------------------------------
        const u64 fields_offset = sizeof(SsType);
        const u64 methods_offset = fields_offset + num_fields * sizeof(SsField);
        const u64 payload_size = methods_offset + num_methods * sizeof(SsMethod);

        ProgramBinaryElement element(payload_size);

        element.m_entry = {
            .m_nameID = StringId(type_name).value, .m_typeId = SS_TYPE_SID, .m_entryPtr = nullptr};

        SsType ss_type{};
        ss_type.m_name = StringId(type_name).value;
        ss_type.m_parent = StringId(type->get_parent()).value;
        ss_type.m_size = static_cast<u32>(st->get_size_in_memory());
        ss_type.m_align = static_cast<u32>(st->get_in_memory_alignment());
        ss_type.m_numFields = num_fields;
        ss_type.m_numMethods = num_methods;
        ss_type.m_pFields = reinterpret_cast<SsField *>(fields_offset);
        ss_type.m_pMethods = (num_methods > 0) ? reinterpret_cast<void *>(methods_offset) : nullptr;
        ss_type.m_flags = 0x2;
        ss_type.m_reserved = 0;

        element.push_value_with_ptr(ss_type, PTR_FIELD(SsType, m_pFields),
                                    PTR_FIELD(SsType, m_pMethods));

        for (const auto &sf : ss_fields) { element.push_value(sf); }

        // m_pLambda is a file offset that the loader must convert to an
        // absolute pointer. Mark it as a relocatable slot, so that
        // BinaryFile::read_reloc_table turns 0x70 into m_bytes + 0x70.
        // Without the reloc bit the value stays a relative offset, and
        // push_call_frame receives a bogus pointer.
        for (const auto &sm : ss_methods) { element.push_value(sm); }

        lg::info("build_ss_type '{}': {} fields, {} methods, {} bytes", type_name, num_fields,
                 num_methods, element.m_rawData.size());

        return element;
    }

    // ============================================================================
    // build_ss_type_for_script
    // ============================================================================
    //
    // Emit the SsType entry for the variable layout of a StateScript.
    // Identical to build_ss_type, except:
    //   * entry name is "<type>$vars";
    //   * m_flags has bit 2 (is_state_vars) set;
    //   * the Field::is_const bit is propagated into SsField::m_flags.
    ProgramBinaryElement
    FileNode::build_ss_type_for_script(const StateScriptNode                      *node,
                                       const std::unordered_map<std::string, u64> &type_lambdas) {
        TypeSystem        &ts = TypeSystem::instance();
        const std::string &type_name = node->type().base_type();

        Type *type = ts.lookup_type_no_throw(type_name);
        if (!type) {
            lg::error("build_ss_type_for_script: unknown type '{}'", type_name);
            return ProgramBinaryElement(0);
        }

        auto *st = dynamic_cast<StructureType *>(type);
        if (!st) {
            lg::error("build_ss_type_for_script: '{}' is not a structure", type_name);
            return ProgramBinaryElement(0);
        }

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
            if (f.is_const()) sf.m_flags |= 0x8;
            sf.m_count = static_cast<u32>(f.array_size());
        }

        const int max_method_id = ts.get_type_method_count(type_name) - 1;
        const u32 num_methods = (max_method_id >= 0) ? static_cast<u32>(max_method_id + 1) : 0;

        std::vector<SsMethod> ss_methods(num_methods);
        for (u32 id = 0; id < num_methods; ++id) {
            MethodInfo info;
            if (!ts.try_lookup_method(type_name, static_cast<int>(id), &info)) {
                ss_methods[id] = {0, 0, nullptr};
                continue;
            }

            SsMethod &sm = ss_methods[id];
            sm.m_name = StringId(info.name).value;

            const std::string full_name = fmt::format("{}-{}", type_name, info.name);
            sm.m_fullName = StringId(full_name).value;

            auto it = type_lambdas.find(info.name);
            sm.m_pLambda =
                (it != type_lambdas.end()) ? reinterpret_cast<ScriptLambda *>(it->second) : nullptr;
        }

        const u64 fields_offset = sizeof(SsType);
        const u64 methods_offset = fields_offset + num_fields * sizeof(SsField);
        const u64 payload_size = methods_offset + num_methods * sizeof(SsMethod);

        ProgramBinaryElement element(payload_size);

        const std::string vars_name = type_name + "$vars";

        element.m_entry = {
            .m_nameID = StringId(vars_name).value,
            .m_typeId = SS_TYPE_SID,
            .m_entryPtr = nullptr,
        };

        SsType ss_type{};
        ss_type.m_name = StringId(vars_name).value;
        ss_type.m_parent = StringId(type->get_parent()).value;
        ss_type.m_size = static_cast<u32>(st->get_size_in_memory());
        ss_type.m_align = static_cast<u32>(st->get_in_memory_alignment());
        ss_type.m_numFields = num_fields;
        ss_type.m_numMethods = num_methods;
        ss_type.m_pFields = reinterpret_cast<SsField *>(fields_offset);
        ss_type.m_pMethods = (num_methods > 0) ? reinterpret_cast<void *>(methods_offset) : nullptr;
        ss_type.m_flags = 0x2 | 0x4; // structure | is_state_vars
        ss_type.m_reserved = 0;

        element.push_value_with_ptr(ss_type, PTR_FIELD(SsType, m_pFields),
                                    PTR_FIELD(SsType, m_pMethods));

        for (const auto &sf : ss_fields) { element.push_value(sf); }
        for (const auto &sm : ss_methods) {
            for (const auto &sm : ss_methods) { element.push_value(sm); }
        }

        lg::info("build_ss_type_for_script '{}': {} fields, {} methods, {} bytes", vars_name,
                 num_fields, num_methods, element.m_rawData.size());

        return element;
    }
    // ============================================================================

    namespace {

        BlockType handler_kind_to_block_type(StateHandler kind) {
            switch (kind) {
            case StateHandler::ENTER: return BlockType::Enter;
            case StateHandler::EXIT: return BlockType::Exit;
            case StateHandler::CODE: return BlockType::Code;
            case StateHandler::UPDATE: return BlockType::Update;
            case StateHandler::POST: return BlockType::Post;
            case StateHandler::EVENT: return BlockType::Event;
            }
            return BlockType::Enter;
        }

    } // namespace

    // ============================================================================
    // build_state_script
    // ============================================================================
    //
    // Emit a StateScript entry and all of its nested arrays. The layout is:
    //
    //   StateScript               (0x50)
    //   SsOptions                 (0x50)
    //   SymbolArray               (0x10)
    //   sid64 options_data[]      (variable)
    //   SsState[]                 (0x18 * num_states)
    //   SsOnBlock[]               (0x50 * num_blocks)         <- all states, flat
    //   SsTrackGroup[]            (0x38 * num_blocks)         <- one per on-block
    //   SsTrack[]                 (0x18 * num_tracks)         <- flat
    //   SsLambda[]                (0x10 * num_lambdas)        <- flat
    //
    // Every pointer is an offset from the start of the file, with a relocation
    // bit set in the file-level bitmap. The ScriptLambda pointers are resolved
    // by name from entry_offsets.
    ProgramBinaryElement
    FileNode::build_state_script(const StateScriptNode                      *node,
                                 const std::unordered_map<std::string, u64> &entry_offsets,
                                 u64 ss_type_file_offset, u64 ss_payload_offset) {
        TypeSystem        &ts = TypeSystem::instance();
        const std::string &type_name = node->type().base_type();

        Type *type = ts.lookup_type_no_throw(type_name);
        if (!type) {
            lg::error("build_state_script: unknown type '{}'", type_name);
            return ProgramBinaryElement(0);
        }

        auto *st = dynamic_cast<StructureType *>(type);
        if (!st) {
            lg::error("build_state_script: '{}' is not a structure", type_name);
            return ProgramBinaryElement(0);
        }

        const auto &opts = type->options();

        // ---- 1. Flatten the state list ----
        //
        // A state is "used" if it has at least one handler OR at least one
        // track. Tracks belong to the state; in the binary they hang off one
        // carrier on-block (see `anchor` / `synth_block`).
        struct StateEntry {
            std::string                           name;
            std::vector<const StateHandlerBody *> handlers;
            const std::vector<TrackInfo>         *tracks = nullptr;
            int                                   anchor = -1; // index in handlers
            bool                                  synth_block = false;

            u32 block_count() const {
                return static_cast<u32>(handlers.size()) + (synth_block ? 1u : 0u);
            }
            u32 track_count() const { return tracks ? static_cast<u32>(tracks->size()) : 0u; }
        };

        auto lambda_count = [](const TrackInfo &tr) -> u32 {
            return (tr.body.empty() ? 0u : 1u) + static_cast<u32>(tr.frames.size());
        };

        std::vector<StateEntry> states;
        for (const auto &[sname, def] : type->state_defs()) {
            if (def.handlers.empty() && def.tracks.empty()) continue;
            StateEntry e;
            e.name = sname;
            for (const auto &h : def.handlers) e.handlers.push_back(&h);
            if (!def.tracks.empty()) e.tracks = &def.tracks;
            states.push_back(std::move(e));
        }

        // Choose the carrier block for each state's tracks.
        for (auto &se : states) {
            if (se.track_count() == 0) continue;
            for (size_t i = 0; i < se.handlers.size(); ++i) {
                if (se.handlers[i]->kind == StateHandler::ENTER) {
                    se.anchor = (int)i;
                    break;
                }
            }
            if (se.anchor < 0) {
                for (size_t i = 0; i < se.handlers.size(); ++i) {
                    if (se.handlers[i]->kind != StateHandler::EVENT) {
                        se.anchor = (int)i;
                        break;
                    }
                }
            }
            if (se.anchor < 0) se.synth_block = true;
        }

        // ---- 2. Count blocks, tracks, lambdas ----
        u32 num_blocks = 0;
        u32 num_tracks = 0;
        u32 num_lambdas = 0;

        for (const auto &se : states) {
            num_blocks += se.block_count();
            num_tracks += se.track_count();
            if (se.tracks) {
                for (const auto &tr : *se.tracks) num_lambdas += lambda_count(tr);
            }
        }

        // ---- 3. Compute offsets ----
        //
        // All pointers are offsets from the start of THIS element's payload.
        // The caller (make_binary) adds the file-relative base later via
        // adjust_offsets().
        const u64 ss_offset = 0;
        const u64 options_offset = ss_offset + sizeof(StateScript);
        const u64 symarray_offset = options_offset + sizeof(SsOptions);

        // Options data: pairs of (key, value) SIDs for every option entry,
        // plus the "initial" value promoted to m_initialStateId.
        std::vector<sid64> option_sids;
        std::string        initial_state;
        {
            auto it = opts.find("initial");
            if (it != opts.end()) initial_state = it->second;
            for (const auto &[k, v] : opts) {
                if (k == "initial") continue;
                option_sids.push_back(static_cast<sid64>(StringId(k)));
                option_sids.push_back(static_cast<sid64>(StringId(v)));
            }
        }
        const u64 options_data_size = option_sids.size() * sizeof(sid64);
        const u64 options_data_offset = symarray_offset + sizeof(SymbolArray);

        const u64 states_offset = options_data_offset + options_data_size;
        const u64 blocks_offset = states_offset + states.size() * sizeof(SsState);

        // SsTrackGroup is inline inside SsOnBlock, so no separate groups region.
        const u64 tracks_offset = blocks_offset + num_blocks * sizeof(SsOnBlock);
        const u64 lambdas_offset = tracks_offset + num_tracks * sizeof(SsTrack);
        const u64 payload_size = lambdas_offset + num_lambdas * sizeof(SsLambda);

        ProgramBinaryElement element(payload_size);

        element.m_entry = {
            .m_nameID = StringId(type_name).value,
            .m_typeId = StringId("state-script").value,
            .m_entryPtr = nullptr,
        };

        // ---- 4. SsOptions ----
        SsOptions ss_options{};
        ss_options.m_optionString = nullptr;
        ss_options.m_unknownFlags = 0;
        ss_options.m_always0_1 = 0;
        ss_options.m_pSymbolArray = reinterpret_cast<SymbolArray *>(symarray_offset);
        ss_options.m_symbolArray2 = nullptr;
        ss_options.m_symbolArray3 = nullptr;
        ss_options.m_symbolArray4 = nullptr;
        ss_options.m_always5 = 5;
        ss_options.m_mostly0 = 0;
        ss_options.m_mostly0Rarely1 = 0;
        ss_options.m_always0_2 = 0;

        SymbolArray symarray{};
        symarray.m_numEntries = static_cast<u32>(option_sids.size());
        symarray.m_unk = 0;
        symarray.m_pSymbols =
            option_sids.empty() ? nullptr : reinterpret_cast<sid64 *>(options_data_offset);

        // ---- 5. SsState[] ----
        std::vector<SsState> ss_states(states.size());
        u32                  block_cursor = 0;
        u32                  track_cursor = 0;
        u32                  lambda_cursor = 0;

        for (size_t si = 0; si < states.size(); ++si) {
            const auto &se = states[si];
            SsState    &sss = ss_states[si];

            sss.m_stateId = static_cast<sid64>(StringId(se.name));
            sss.m_numSsOnBlocks = static_cast<i64>(se.block_count());
            sss.m_pSsOnBlocks = reinterpret_cast<SsOnBlock *>(
                blocks_offset + si * 0 /* replaced below */); // set after all states are sized

            // Blocks for this state start at block_cursor. The pointer above is
            // recomputed once we know the actual global block index. For now,
            // stash the per-state block start in a parallel array.
        }

        // Compute per-state block starts (blocks are laid out in state order).
        std::vector<u64> state_block_starts(states.size());
        {
            u32 cursor = 0;
            for (size_t si = 0; si < states.size(); ++si) {
                state_block_starts[si] = blocks_offset + cursor * sizeof(SsOnBlock);
                cursor += states[si].block_count();
            }
        }

        for (size_t si = 0; si < states.size(); ++si) {
            ss_states[si].m_pSsOnBlocks = reinterpret_cast<SsOnBlock *>(state_block_starts[si]);
        }

        // ---- 6. SsOnBlock[], SsTrackGroup[], SsTrack[], SsLambda[] ----
        std::vector<SsOnBlock>    ss_blocks(num_blocks);
        std::vector<SsTrack>      ss_tracks(num_tracks);
        std::vector<SsLambda>     ss_lambdas(num_lambdas);

        auto lookup_lambda = [&](const std::string &name) -> ScriptLambda * {
            auto it = entry_offsets.find(name);
            if (it == entry_offsets.end()) {
                fmt::print(stderr, "DEBUG lookup_lambda: '{}' NOT FOUND\n", name);
                return nullptr;
            }
            // entry_offsets gives ABSOLUTE file offsets. The slot must contain
            // a RELATIVE offset (from the start of this StateScript payload),
            // because adjust_offsets() will add the payload base and the loader
            // will add the file base. Subtract the payload offset here.
            const u64 absolute = it->second;
            return reinterpret_cast<ScriptLambda *>(absolute - ss_payload_offset);
        };

        block_cursor = 0;
        track_cursor = 0;
        lambda_cursor = 0;

        // Fill a track group from the state's tracks (or leave it empty).
        auto fill_group = [&](const StateEntry &se, SsTrackGroup &grp, bool with_tracks) {
            grp.m_always0 = 0;
            grp.m_padding = 0;
            grp.m_name = nullptr;
            grp.m_always0_1 = 0;
            grp.m_always0_2 = 0;
            grp.m_rareScriptLambda = nullptr;

            if (!with_tracks || se.track_count() == 0) {
                grp.m_totalLambdaCount = 0;
                grp.m_numTracks = 0;
                grp.m_aTracks = nullptr;
                return;
            }

            const auto &trs = *se.tracks;
            u32         group_lambdas = 0;
            for (const auto &tr : trs) group_lambdas += lambda_count(tr);

            grp.m_totalLambdaCount = static_cast<u16>(group_lambdas);
            grp.m_numTracks = static_cast<i16>(trs.size());
            grp.m_aTracks =
                reinterpret_cast<SsTrack *>(tracks_offset + track_cursor * sizeof(SsTrack));

            u32 local_lambda = 0;
            for (size_t ti = 0; ti < trs.size(); ++ti) {
                const auto &tr = trs[ti];
                SsTrack    &t = ss_tracks[track_cursor];
                const u32   n = lambda_count(tr);

                t.m_trackId = static_cast<sid64>(StringId(tr.name));
                t.m_trackIdx = static_cast<u16>(ti);
                t.m_totalLambdaCount = static_cast<i16>(n);
                t.m_padding = 0;
                t.m_pSsLambda =
                    n > 0 ? reinterpret_cast<SsLambda *>(
                                lambdas_offset + (lambda_cursor + local_lambda) * sizeof(SsLambda))
                          : nullptr;

                const std::string prefix = type_name + "-" + se.name + "-track-" + tr.name;
                u32               li = lambda_cursor + local_lambda;

                // On-begin coroutine: counter = -1.
                if (!tr.body.empty()) {
                    ss_lambdas[li++] = {lookup_lambda(prefix + "-main"), static_cast<u64>(-1)};
                }
                // Reactive frame triggers: counter = N.
                for (size_t fi = 0; fi < tr.frames.size(); ++fi) {
                    ss_lambdas[li++] = {lookup_lambda(prefix + "-frame-" + std::to_string(fi)),
                                        static_cast<u64>(tr.frames[fi].frame)};
                }

                local_lambda += n;
                ++track_cursor;
            }
            lambda_cursor += group_lambdas;
        };

        for (size_t si = 0; si < states.size(); ++si) {
            const auto &se = states[si];

            for (size_t hi = 0; hi < se.handlers.size(); ++hi) {
                const StateHandlerBody *h = se.handlers[hi];

                SsOnBlock &blk = ss_blocks[block_cursor];

                blk.m_blockType = handler_kind_to_block_type(h->kind);
                blk.m_always0 = 0;
                blk.m_blockEventId =
                    h->kind == StateHandler::EVENT ? static_cast<sid64>(StringId(h->event_id)) : 0;

                // Handler body: <type>-<state>-<kind>[-event-<id>]
                std::string body_name =
                    type_name + "-" + se.name + "-" + state_handler_to_string(h->kind);
                if (h->kind == StateHandler::EVENT) body_name += "-event-" + h->event_id;
                blk.m_pScriptLambda = lookup_lambda(body_name);

                fill_group(se, blk.m_trackGroup, static_cast<int>(hi) == se.anchor);
                ++block_cursor;
            }

            // Synthetic carrier: state has tracks but no suitable handler.
            if (se.synth_block) {
                SsOnBlock &blk = ss_blocks[block_cursor];
                blk.m_blockType = BlockType::Enter;
                blk.m_always0 = 0;
                blk.m_blockEventId = 0;
                blk.m_pScriptLambda = nullptr;
                fill_group(se, blk.m_trackGroup, true);
                ++block_cursor;
            }
        }

        // ---- 7. StateScript header ----
        StateScript ss{};
        ss.m_stateScriptId = static_cast<sid64>(StringId(type_name));
        // m_pVarType is patched by make_binary after all payloads are laid out.
        // Storing 0 here keeps the reloc bit meaningful: adjust_offsets will add
        // the StateScript payload base to whatever relative value we write there.
        ss.m_pVarType = reinterpret_cast<SsType *>(0);
        ss.m_initialStateId =
            initial_state.empty() ? 0 : static_cast<sid64>(StringId(initial_state));
        ss.m_pSsOptions = reinterpret_cast<SsOptions *>(options_offset);
        ss.m_pVarDefaults = nullptr; // filled by make_binary via blob pool
        ss.m_pSsStateTable = states.empty() ? nullptr : reinterpret_cast<SsState *>(states_offset);
        ss.m_stateCount = static_cast<i16>(states.size());
        ss.m_line = 0;
        ss.m_always0_2 = 0;
        ss.m_pDebugFileName = nullptr; // filled by make_binary
        ss.m_pErrorName = nullptr;
        ss.m_padding = 0;

        // ---- 8. Serialize ----
        element.push_value_with_ptr(
            ss, PTR_FIELD(StateScript, m_pVarType), PTR_FIELD(StateScript, m_pSsOptions),
            PTR_FIELD(StateScript, m_pVarDefaults), PTR_FIELD(StateScript, m_pSsStateTable),
            PTR_FIELD(StateScript, m_pDebugFileName), PTR_FIELD(StateScript, m_pErrorName));

        element.push_value_with_ptr(
            ss_options, PTR_FIELD(SsOptions, m_pSymbolArray), PTR_FIELD(SsOptions, m_symbolArray2),
            PTR_FIELD(SsOptions, m_symbolArray3), PTR_FIELD(SsOptions, m_symbolArray4));

        element.push_value_with_ptr(symarray, PTR_FIELD(SymbolArray, m_pSymbols));

        for (sid64 s : option_sids) { element.push_value(s); }

        for (const auto &s : ss_states) {
            element.push_value_with_ptr(s, PTR_FIELD(SsState, m_pSsOnBlocks));
        }

        for (const auto &b : ss_blocks) {
            element.push_value_with_ptr(
                b, 
                PTR_FIELD(SsOnBlock, m_pScriptLambda),
                PtrSlot{offsetof(SsOnBlock, m_trackGroup) + offsetof(SsTrackGroup, m_aTracks),
                        sizeof(SsTrack *)},
                PtrSlot{offsetof(SsOnBlock, m_trackGroup) + offsetof(SsTrackGroup, m_name),
                        sizeof(const char *)},
                PtrSlot{offsetof(SsOnBlock, m_trackGroup) +
                            offsetof(SsTrackGroup, m_rareScriptLambda),
                        sizeof(const ScriptLambda *)});
        }

        for (const auto &t : ss_tracks) {
            element.push_value_with_ptr(t, PTR_FIELD(SsTrack, m_pSsLambda));
        }

        for (const auto &l : ss_lambdas) {
            element.push_value_with_ptr(l, PTR_FIELD(SsLambda, m_pScriptLambda));
        }

        lg::info("build_state_script '{}': {} states, {} blocks, {} tracks, {} lambdas, {} bytes",
                 type_name, states.size(), num_blocks, num_tracks, num_lambdas,
                 element.m_rawData.size());

        return element;
    }

    // ============================================================================
    // make_unique_anon_name for unamed structures
    // ============================================================================

    std::string FileNode::make_unique_anon_name(const std::string &base) {
        if (!has_local(base)) { return base; }
        u32 &n = m_anon_name_counter[base];
        return fmt::format("{}#{}", base, ++n);
    }

    // ============================================================================
    // make_binary - сборка финального бинарника
    // ============================================================================
    ProgramBinaryElement FileNode::make_binary(std::vector<ProgramBinaryElement> program_elements,
                                               DataSegment                      &data_segment,
                                               std::vector<DataStructEntry>     &out_data_structs) {
        lg::debug("make_binary: program_elements.size()={}", program_elements.size());

        constexpr sid64 ARRAY_SID = SID("array");
        constexpr u64   first_entry_offset = 0x28;
        constexpr u32   header_size = sizeof(DC_Header) + sizeof(ARRAY_SID);

        // ------------------------------------------------------------------
        // Collect type declarations from m_children. They are emitted as
        // SsType entries in the entry table, so we must know about them
        // before we can bail out on an empty program_elements list.
        // ------------------------------------------------------------------
        struct TypeInfo {
            const TypeDeclarationNode           *decl;
            std::unordered_map<std::string, u64> method_offsets;
        };
        std::vector<TypeInfo> type_infos;
        for (auto &child : m_children) {
            if (auto *td = dynamic_cast<TypeDeclarationNode *>(child.get())) {
                TypeInfo ti;
                ti.decl = td;
                type_infos.push_back(std::move(ti));
            }
        }

        // State-script nodes: each produces TWO entries — SsType (under
        // "<type>$vars") and StateScript (under "<type>").
        struct StateScriptInfo {
            const StateScriptNode               *node;
            std::unordered_map<std::string, u64> method_offsets;
            u64                                  ss_type_offset = 0;
        };
        std::vector<StateScriptInfo> state_script_infos;
        for (auto &child : m_children) {
            if (auto *ss = dynamic_cast<StateScriptNode *>(child.get())) {
                StateScriptInfo si;
                si.node = ss;
                state_script_infos.push_back(std::move(si));
            }
        }

        if (program_elements.empty() && type_infos.empty() && state_script_infos.empty()) {
            return ProgramBinaryElement(0);
        }

        // ------------------------------------------------------------------
        // Pad every element's raw data to a multiple of 8 bytes.
        // ------------------------------------------------------------------
        for (auto &el : program_elements) {
            while (el.m_rawData.size() % 8 != 0) { el.m_rawData.push_back(std::byte{0}); }
            const size_t slots = (el.m_rawData.size() + 7) / 8;
            while (el.m_relocTable.size() < slots) { el.m_relocTable.push_back(false); }
            el.check_size();
        }

        // total_entries must count every entry that will end up in the
        // entry table: program_elements, type_infos (SsType per type),
        // state_script_infos (SsType + StateScript per state-script),
        // and m_placeholders (one per heap-allocated top-level define).
        //
        // m_placeholders are appended at the very end of make_binary, but
        // they still occupy a DCEntry slot. If they are not counted here,
        // running_offset for the first function is off by
        // placeholders.size() * sizeof(DCEntry), and every SsMethod::m_pLambda
        // points into the wrong place.
        // Every entry that will end up in the table: functions, SsTypes
        // (one per type), SsType+StateScript (two per state-script),
        // heap placeholders, and the "requires" entry (one, if any
        // dependencies were recorded).
        const u64 total_entries = program_elements.size() + type_infos.size() +
                                  state_script_infos.size() * 2 + m_placeholders.size() +
                                  (m_requires.empty() ? 0 : 1);

        // ------------------------------------------------------------------
        // Pass 1: method offsets for SsType entries.
        // ------------------------------------------------------------------
        {
            u64 running_offset = header_size + total_entries * sizeof(DCEntry);

            for (auto &el : program_elements) {
                const std::string el_name =
                    StringIdManager::instance().get_string(el.m_entry.m_nameID);

                for (auto &ti : type_infos) {
                    const std::string &tname = ti.decl->type().base_type();
                    const std::string  prefix = tname + "-";
                    if (el_name.rfind(prefix, 0) == 0) {
                        ti.method_offsets[el_name.substr(prefix.size())] = running_offset;
                    }
                }

                for (auto &si : state_script_infos) {
                    const std::string &tname = si.node->type().base_type();
                    const std::string  prefix = tname + "-";
                    if (el_name.rfind(prefix, 0) == 0) {
                        si.method_offsets[el_name.substr(prefix.size())] = running_offset;
                    }
                }

                running_offset += el.m_rawData.size();
            }
        }

        // ------------------------------------------------------------------
        // Append SsType entries for plain TypeDeclarationNodes.
        // ------------------------------------------------------------------
        for (auto &ti : type_infos) {
            ProgramBinaryElement ss_el = build_ss_type(ti.decl, ti.method_offsets);
            if (!ss_el.m_rawData.empty()) { program_elements.push_back(std::move(ss_el)); }
        }

        // ------------------------------------------------------------------
        // Append SsType entries for StateScriptNodes (under "<type>$vars").
        // Record each one's position in program_elements so we can turn it
        // into a file offset before emitting the StateScript entries.
        // ------------------------------------------------------------------
        for (auto &si : state_script_infos) {
            ProgramBinaryElement ss_el = build_ss_type_for_script(si.node, si.method_offsets);
            if (!ss_el.m_rawData.empty()) {
                si.ss_type_offset = program_elements.size(); // index, resolved below
                program_elements.push_back(std::move(ss_el));
            }
        }

        // ------------------------------------------------------------------
        // Convert the recorded indices into file offsets.
        // ------------------------------------------------------------------
        {
            const u64 current_num_entries = program_elements.size();
            u64       running = header_size + current_num_entries * sizeof(DCEntry);

            for (size_t i = 0; i < program_elements.size(); ++i) {
                for (auto &si : state_script_infos) {
                    if (si.ss_type_offset == i) { si.ss_type_offset = running; }
                }
                running += program_elements[i].m_rawData.size();
            }
        }

        // ------------------------------------------------------------------
        // Append StateScript entries.
        // ------------------------------------------------------------------
        {
            // Compute entry offsets for all entries that exist NOW (functions,
            // data, plain SsTypes, and SsType-for-script). StateScript entries
            // are appended next and do not participate in this map, because
            // ScriptLambda references from SsOnBlock/SsLambda always point to
            // function entries that are already placed.
            std::unordered_map<std::string, u64> tmp_entry_offsets;
            u64 running = header_size + program_elements.size() * sizeof(DCEntry);
            for (auto &el : program_elements) {
                const std::string name =
                    StringIdManager::instance().get_string(el.m_entry.m_nameID);
                tmp_entry_offsets[name] = running;
                running += el.m_rawData.size();
            }

            for (auto &si : state_script_infos) {
                // ss_payload_offset is the offset at which this StateScript's payload
                // will start in the final file. It is exactly the current running
                // offset, because the element is appended next.
                ProgramBinaryElement ss_el =
                    build_state_script(si.node, tmp_entry_offsets, si.ss_type_offset, running);
                if (!ss_el.m_rawData.empty()) {
                    running += ss_el.m_rawData.size();
                    program_elements.push_back(std::move(ss_el));
                }
            }
        }

        // ------------------------------------------------------------------
        // Placeholder entries for heap-allocated top-level defines.
        // ------------------------------------------------------------------
        for (const std::string &pname : m_placeholders) {
            ProgramBinaryElement placeholder(8);
            placeholder.m_entry = {
                .m_nameID = StringId(pname).value,
                .m_typeId = StringId("global").value,
                .m_entryPtr = nullptr,
            };
            const u64 zero = 0;
            placeholder.push_blob(&zero, sizeof(zero), /*relocation_bit=*/0);
            program_elements.push_back(std::move(placeholder));
            lg::debug("make_binary: placeholder entry '{}'", pname);
        }

        // ------------------------------------------------------------------
        // "requires" entry: list of modules this file depends on.
        // ------------------------------------------------------------------
        //
        // Layout:
        //   SymbolArray  (0x10)
        //   sid64[]      (8 * N)
        //
        // The SymbolArray itself is the entry payload; m_pSymbols points
        // at the sid64 array that immediately follows, and is marked as
        // relocatable so that the loader turns the file offset into an
        // absolute pointer.
        //
        // The entry is emitted only if there is at least one dependency.
        // A file with no requires has no "requires" entry at all, which
        // the inspector reports as "(none)".
        if (!m_requires.empty()) {
            const u32 n = static_cast<u32>(m_requires.size());

            // Layout: RequiresList (0x10) + const char*[N] (8 * N).
            // m_pNames points at the pointer array that follows. Each
            // m_pNames[i] points at a string in the string pool; the
            // string itself is added via add_string() and resolved by
            // m_stringConstantSlots at Pass 3a.
            const u64 names_offset = sizeof(RequiresList);
            const u64 payload_size = names_offset + n * sizeof(const char *);

            ProgramBinaryElement req_el(payload_size);
            req_el.m_entry = {
                .m_nameID = StringId("requires").value,
                .m_typeId = StringId("requires").value,
                .m_entryPtr = nullptr,
            };

            RequiresList list{};
            list.m_numEntries = n;
            list.m_reserved = 0;
            list.m_pNames = reinterpret_cast<const char **>(names_offset);

            req_el.push_value_with_ptr(list, PTR_FIELD(RequiresList, m_pNames));

            for (u32 i = 0; i < n; ++i) {
                const u32 str_offset = data_segment.add_string(m_requires[i]);

                const u64 slot_offset = names_offset + i * sizeof(const char *);
                req_el.m_stringConstantSlots.push_back({slot_offset, str_offset});

                // Placeholder: make_binary::Pass 3a writes the absolute
                // string-pool offset into this slot.
                const u64 offset_in_payload = req_el.m_rawData.size();
                req_el.push_value(static_cast<u64>(0));

                // Mark the slot as relocatable. Pass 3a clears the bit
                // before adjust_offsets() runs and restores it afterwards,
                // so the loader turns the file-relative string offset into
                // an absolute pointer. Without this, the slot stays 0 and
                // the loader sees a null m_pNames[i].
                const size_t slot_bit = offset_in_payload / 8;
                if (slot_bit < req_el.m_relocTable.size()) { req_el.m_relocTable[slot_bit] = true; }
            }

            program_elements.push_back(std::move(req_el));
            lg::debug("make_binary: requires entry with {} deps", n);
        }
        // ================================================================
        // Pass 2: file layout.
        // ================================================================
        //
        // Layout from start of file:
        //     [header]
        //     [ARRAY_SID]
        //     [entry table]
        //     [entry payloads]
        //     [string pool]      <- m_stringsOffset
        //     [pad to 4]
        //     [reloc table]      <- m_textSize
        //
        const u64 num_entries = program_elements.size();
        const u64 entries_size = sizeof(DCEntry) * num_entries;

        const u64 entries_data_size =
            std::accumulate(program_elements.begin(), program_elements.end(), u64{0},
                            [](u64 acc, const ProgramBinaryElement &element) {
                                return acc + element.m_rawData.size();
                            });

        const auto &string_pool_bytes = data_segment.strings().bytes();

        const u64 strings_size = string_pool_bytes.size();

        const u64 data_size = header_size + entries_size + entries_data_size;
        const u64 strings_start = data_size;
        // Pad to 4, not 8: the only section after the string pool is the
        // relocation table, and its u32 size field needs 4-byte alignment.
        const u64 strings_pad = (4 - (strings_size % 4)) % 4;
        const u64 relocatable_size = strings_start + strings_size + strings_pad;
        const u64 reloc_bytes = (relocatable_size + 7) / 8;
        const u64 total_size = relocatable_size + 4 + reloc_bytes;

        lg::debug("make_binary: total_size={}, strings_start={}", total_size, strings_start);

        ProgramBinaryElement element(total_size);

        // ========================================
        // 1. HEADER
        // ========================================
        DC_Header header{DC_FILE_MAGIC,
                         DC_FILE_VERSION,
                         static_cast<uint32_t>(relocatable_size), // m_textSize
                         static_cast<uint32_t>(strings_start),    // m_stringsOffset
                         0,                                       // m_reserved (was m_blobsOffset)
                         static_cast<uint32_t>(num_entries),      // m_numEntries
                         reinterpret_cast<DCEntry *>(first_entry_offset)};

        element.push_value_with_ptr(header, PTR_FIELD(DC_Header, m_pStartOfData));
        element.push_value(ARRAY_SID);

        // ========================================
        // 2. ENTRY TABLE
        // ========================================
        const u64 first_function_start = header_size + num_entries * sizeof(DCEntry);
        u64       prev_entry_size = 0;

        out_data_structs.clear();

        // Map entry name -> file offset, filled during the entry-table
        // pass. Used by the NamedStatic struct-pointer resolver below.
        std::unordered_map<std::string, u64> entry_offsets;

        for (auto &fn : program_elements) {
            DCEntry entry = fn.m_entry;
            entry.m_entryPtr = reinterpret_cast<void *>(first_function_start + prev_entry_size);

            const std::string entry_name = StringIdManager::instance().get_string(entry.m_nameID);
            entry_offsets[entry_name] = first_function_start + prev_entry_size;

            element.push_value_with_ptr(entry, PTR_FIELD(DCEntry, m_entryPtr));
            prev_entry_size += fn.m_rawData.size();

            if (fn.m_structLayout) {
                DataStructEntry ds;
                ds.name = StringIdManager::instance().get_string(entry.m_nameID);
                ds.type_name = fn.m_structLayout->type_name;
                ds.offset = reinterpret_cast<u64>(entry.m_entryPtr);
                ds.layout = *fn.m_structLayout;
                out_data_structs.push_back(std::move(ds));
            }
        }
        // ------------------------------------------------------------------
        // Patch StateScript::m_pVarType with the final SsType offset.
        // ------------------------------------------------------------------
        //
        // At this point entry_offsets holds the final file offset of every
        // entry (including StateScripts). For each StateScript entry we
        // look up the associated "<type>$vars" SsType entry and the
        // StateScript's own payload offset, then write the relative
        // (ss_type - payload) into the m_pVarType slot. adjust_offsets()
        // below will add the payload base back, yielding the absolute
        // pointer the runtime expects.
        for (auto &el : program_elements) {
            if (el.m_entry.m_typeId != StringId("state-script").value) continue;

            const std::string entry_name =
                StringIdManager::instance().get_string(el.m_entry.m_nameID);
            const std::string vars_name = entry_name + "$vars";

            auto ss_it = entry_offsets.find(entry_name);
            auto vars_it = entry_offsets.find(vars_name);
            if (ss_it == entry_offsets.end() || vars_it == entry_offsets.end()) continue;

            const u64 payload_absolute = ss_it->second;
            const u64 ss_type_absolute = vars_it->second;

            constexpr size_t var_type_offset = offsetof(StateScript, m_pVarType);
            if (var_type_offset + sizeof(u64) > el.m_rawData.size()) continue;

            *reinterpret_cast<u64 *>(&el.m_rawData[var_type_offset]) =
                ss_type_absolute - payload_absolute;

            // build_state_script wrote m_pVarType = nullptr and, because
            // push_value_with_ptr skips nullptrs, the slot is not marked
            // as relocatable. We just wrote a non-zero relative offset,
            // so we must set the bit ourselves; adjust_offsets() below
            // will then add the payload base, yielding the absolute
            // pointer the runtime expects.
            const size_t slot_index = var_type_offset / 8;
            if (slot_index < el.m_relocTable.size()) { el.m_relocTable[slot_index] = true; }
        }
        // ========================================
        // 3. PAYLOADS
        // ========================================
        std::vector<bool> file_reloc_table(relocatable_size, false);

        for (auto &fn : program_elements) {
            // ---- 3a. String slots ----
            //
            // Write the absolute file offset (strings_start + in-pool offset),
            // then temporarily clear the reloc bit so adjust_offsets() does
            // not add function_start to the value.
            for (const auto &slot : fn.m_stringConstantSlots) {
                const u64 absolute = strings_start + slot.str_offset_in_table;
                *reinterpret_cast<u64 *>(&fn.m_rawData[slot.slot_offset]) = absolute;
            }
            std::vector<bool> saved_string_bits(fn.m_stringConstantSlots.size(), false);
            for (size_t i = 0; i < fn.m_stringConstantSlots.size(); ++i) {
                const size_t slot_bit = fn.m_stringConstantSlots[i].slot_offset / 8;
                if (slot_bit < fn.m_relocTable.size()) {
                    saved_string_bits[i] = fn.m_relocTable[slot_bit];
                    fn.m_relocTable[slot_bit] = false;
                }
            }

            // ---- 3b. Struct pointer slots ----
            //
            // Write the absolute file offset (blobs_start + in-pool offset).
            // Leave the reloc bit clear for now — adjust_offsets() must not
            // touch these; we set the bit after adjust_offsets() runs.
            for (const auto &slot : fn.m_structPointerSlots) {
                u64 absolute = 0;

                switch (slot.kind) {
                case ProgramBinaryElement::StructPointerSlot::Kind::NamedStatic: {
                    // Try to resolve the target name to a file offset
                    // in this entry table. If the name is not present,
                    // the pointer is a cross-module reference: the
                    // compiler already created a StoreFieldNode to
                    // write it at runtime, and the slot stays 0 here.
                    auto it = entry_offsets.find(slot.target_name);
                    if (it == entry_offsets.end()) {
                        absolute = 0;
                        break;
                    }
                    absolute = it->second;
                    break;
                }
                }

                *reinterpret_cast<u64 *>(&fn.m_rawData[slot.slot_offset]) = absolute;

                const size_t slot_bit = slot.slot_offset / 8;
                if (slot_bit < fn.m_relocTable.size()) { fn.m_relocTable[slot_bit] = false; }
            }

            // ---- 3c. adjust_offsets ----
            const u64 function_start = element.m_rawData.size();
            fn.adjust_offsets(function_start);
            // ---- 3c'. Mark SsMethod::m_pLambda as relocatable ----
            if (fn.m_entry.m_typeId == SS_TYPE_SID) {
                auto *hdr = reinterpret_cast<SsType *>(fn.m_rawData.data());
                if (hdr && hdr->m_numMethods > 0) {
                    const u64 methods_offset =
                        sizeof(SsType) + static_cast<u64>(hdr->m_numFields) * sizeof(SsField);

                    //fmt::print(stderr,
                    //           "3c' entry='{}' payload={} numFields={} numMethods={} "
                    //           "methods_offset=0x{:X} sizeof(SsType)={} sizeof(SsField)={} "
                    //           "sizeof(SsMethod)={} offsetof_pLambda={}\n",
                    //           StringIdManager::instance().get_string(fn.m_entry.m_nameID),
                    //           fn.m_rawData.size(), hdr->m_numFields, hdr->m_numMethods,
                    //           methods_offset, sizeof(SsType), sizeof(SsField), sizeof(SsMethod),
                    //           offsetof(SsMethod, m_pLambda));

                    for (u32 i = 0; i < hdr->m_numMethods; ++i) {
                        const u64 slot =
                            methods_offset + i * sizeof(SsMethod) + offsetof(SsMethod, m_pLambda);
                        //fmt::print(stderr, "   i={} slot=0x{:X} payload={} ok={}\n", i, slot,
                        //           fn.m_rawData.size(), slot + 8 <= fn.m_rawData.size());
                        if (slot + 8 > fn.m_rawData.size()) break;
                        const size_t slot_bit = slot / 8;
                        if (slot_bit < fn.m_relocTable.size()) { fn.m_relocTable[slot_bit] = true; }
                    }
                }
            }
            // ---- 3d. Restore string bits ----
            for (size_t i = 0; i < fn.m_stringConstantSlots.size(); ++i) {
                const size_t slot_bit = fn.m_stringConstantSlots[i].slot_offset / 8;
                if (slot_bit < fn.m_relocTable.size() && saved_string_bits[i]) {
                    fn.m_relocTable[slot_bit] = true;
                }
            }

            // ---- 3e. Set struct pointer bits (they contain absolute
            //          offsets already, and must be relocated by the loader) ----
            for (const auto &slot : fn.m_structPointerSlots) {
                const size_t slot_bit = slot.slot_offset / 8;
                if (slot_bit < fn.m_relocTable.size()) { fn.m_relocTable[slot_bit] = true; }
            }

            // ---- 3f. Append payload ----
            element.push_blob(fn.m_rawData.data(), fn.m_rawData.size(), /*relocation_bit=*/0);

            // ---- 3g. Fold per-element bits into file-level bitmap ----
            const size_t function_slot = function_start / 8;
            for (size_t i = 0; i < fn.m_relocTable.size(); ++i) {
                if (!fn.m_relocTable[i]) { continue; }
                const size_t file_slot = function_slot + i;

                if (file_slot < element.m_relocTable.size()) {
                    element.m_relocTable[file_slot] = true;
                }
                if (file_slot < file_reloc_table.size()) { file_reloc_table[file_slot] = true; }
            }
        }

        // ========================================
        // 4. STRING POOL
        // ========================================
        if (strings_size > 0) {
            element.push_blob(string_pool_bytes.data(), strings_size,
                              /*relocation_bit=*/0);
        }
        for (u64 i = 0; i < strings_pad; ++i) {
            const std::byte zero = std::byte{0};
            element.push_blob(&zero, 1, /*relocation_bit=*/0);
        }

        // ========================================
        // 6. RELOCATION TABLE
        // ========================================
        for (size_t i = 0; i < element.m_relocTable.size() && i < file_reloc_table.size(); ++i) {
            if (element.m_relocTable[i]) { file_reloc_table[i] = true; }
        }

        const u32 reloc_size = static_cast<u32>(reloc_bytes);
        element.push_value(reloc_size);

        for (u64 i = 0; i < reloc_bytes; ++i) {
            uint8_t byte = 0;
            for (size_t bit = 0; bit < 8; ++bit) {
                const u64 idx = i * 8 + bit;
                if (idx < file_reloc_table.size() && file_reloc_table[idx]) { byte |= (1 << bit); }
            }
            element.push_value(byte);
        }

        if (element.m_rawData.size() != total_size) {
            throw std::runtime_error(
                fmt::format("FileNode::make_binary: assembled {} bytes but expected {}. "
                            "Header text/string offsets will be wrong.",
                            element.m_rawData.size(), total_size));
        }
        
        return element;
    }

    // ============================================================================
    // Управление символами
    // ============================================================================
    Node *FileNode::lookup(const std::string &name) {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end()) return it->second;

        for (auto *imp : m_imports) {
            if (auto *val = imp->lookup(name)) return val;
        }

        return parent() ? parent()->lookup(name) : nullptr;
    }

    void FileNode::bind(const std::string &name, Node *node) {
        if (m_symbols.find(name) == m_symbols.end()) { m_ordered_symbols.push_back(node); }
        m_symbols[name] = node;
    }

    /// @brief Remove any child FunctionNode or DataDeclarationNode
    ///        with the given name, and drop its binding.
    ///
    /// @details Used by build_define for redefinition in REPL mode.
    ///          The child's destructor runs, but any shared_ptr to
    ///          the same NewNode (held by a StoreGlobalNode) keeps
    ///          the NewNode alive.
    ///
    ///          m_ordered_symbols is rebuilt from scratch after the
    ///          erase. It is a small vector and only used by
    ///          take_symbols() at the very end of a session, so the
    ///          rebuild cost is irrelevant.
    void FileNode::remove_named(const std::string &name) {
        m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
                                        [&](const std::unique_ptr<Node> &c) {
                                            if (auto *fn = dynamic_cast<FunctionNode *>(c.get())) {
                                                return fn->name() == name;
                                            }
                                            if (auto *dd =
                                                    dynamic_cast<DataDeclarationNode *>(c.get())) {
                                                return dd->name() == name;
                                            }
                                            return false;
                                        }),
                         m_children.end());

        m_symbols.erase(name);

        // Rebuild m_ordered_symbols from m_symbols. The old vector
        // may contain dangling Node* to children just erased.
        m_ordered_symbols.clear();
        m_ordered_symbols.reserve(m_symbols.size());
        for (auto &[n, p] : m_symbols) {
            (void)n;
            m_ordered_symbols.push_back(p);
        }
    }

    void FileNode::add_import(FileNode *file) {
        if (!file || file == this) return;
        if (std::find(m_imports.begin(), m_imports.end(), file) == m_imports.end()) {
            m_imports.push_back(file);
        }
    }

    void FileNode::insert_into_reloctable(u8 *reloc_table, u64 &byte_offset, u64 &bit_offset,
                                          u8 bits, u64 num_bits) noexcept {
        const u8 bit_space_remaining = (8 - bit_offset % 8);
        if (bit_space_remaining >= num_bits) {
            reloc_table[byte_offset] |= bits << bit_offset;
            bit_offset += num_bits;
            if (bit_offset == 8) {
                bit_offset = 0;
                byte_offset++;
            }
        } else {
            reloc_table[byte_offset++] |= bits << bit_offset;
            reloc_table[byte_offset] |= bits >> bit_space_remaining;
            bit_offset = num_bits - bit_space_remaining;
        }
    }

} // namespace sootc