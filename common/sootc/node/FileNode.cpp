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
        return "FileNode(name=" + m_name + ", symbols=" + std::to_string(m_symbols.size()) + ")";
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
                if (payload_size > 0) {
                    entries.push_back(std::move(element));
                    lg::debug("Data declaration '{}': {} bytes", decl->name(), payload_size);
                } else if (decl->is_heap()) {
                    // Heap-allocated declarations have no payload by
                    // design. The slot in the file is a placeholder
                    // emitted by make_binary from m_placeholders, and
                    // the pointer is written at runtime by a
                    // StoreGlobalNode in the top-level body. Silence
                    // is correct here, and it also avoids spamming the
                    // log on every REPL input (this child is visited
                    // on every generate() call for the whole session).
                    lg::debug("Data declaration '{}': heap, no payload", decl->name());
                } else {
                    lg::warn("Data declaration '{}' produced empty element", decl->name());
                }
            }
        }

        return entries;
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

        // m_pLambda is already an absolute file offset (computed in Pass 1),
        // so it must NOT be marked as relocatable.
        for (const auto &sm : ss_methods) { element.push_value(sm); }

        lg::info("build_ss_type '{}': {} fields, {} methods, {} bytes", type_name, num_fields,
                 num_methods, element.m_rawData.size());

        return element;
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

        if (program_elements.empty() && type_infos.empty()) { return ProgramBinaryElement(0); }

        // ------------------------------------------------------------------
        // Pad every element's raw data to a multiple of 8 bytes.
        // ------------------------------------------------------------------
        for (auto &el : program_elements) {
            while (el.m_rawData.size() % 8 != 0) { el.m_rawData.push_back(std::byte{0}); }
            const size_t slots = (el.m_rawData.size() + 7) / 8;
            while (el.m_relocTable.size() < slots) { el.m_relocTable.push_back(false); }
            el.check_size();
        }

        const u64 total_entries = program_elements.size() + type_infos.size();

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
                        const std::string method_name = el_name.substr(prefix.size());
                        ti.method_offsets[method_name] = running_offset;
                    }
                }

                running_offset += el.m_rawData.size();
            }
        }

        // ------------------------------------------------------------------
        // Append SsType entries.
        // ------------------------------------------------------------------
        for (auto &ti : type_infos) {
            ProgramBinaryElement ss_el = build_ss_type(ti.decl, ti.method_offsets);
            if (!ss_el.m_rawData.empty()) { program_elements.push_back(std::move(ss_el)); }
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
        //     [pad to 8]
        //     [blob pool]        <- m_blobsOffset (0 if empty)
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
        const auto &blob_pool_bytes = data_segment.blobs().bytes();

        const u64 strings_size = string_pool_bytes.size();
        const u64 blobs_size = blob_pool_bytes.size();

        const u64 data_size = header_size + entries_size + entries_data_size;
        const u64 strings_start = data_size;
        const u64 strings_pad = (8 - (strings_size % 8)) % 8;
        const u64 blobs_start = strings_start + strings_size + strings_pad;
        const u64 blobs_pad = (4 - (blobs_size % 4)) % 4;
        const u64 relocatable_size = blobs_start + blobs_size + blobs_pad;
        const u64 reloc_bytes = (relocatable_size + 7) / 8;
        const u64 total_size = relocatable_size + 4 + reloc_bytes;

        lg::debug("make_binary: total_size={}, strings_start={}, blobs_start={}", total_size,
                  strings_start, blobs_start);

        ProgramBinaryElement element(total_size);

        // ========================================
        // 1. HEADER
        // ========================================
        DC_Header header{DC_FILE_MAGIC,
                         DC_FILE_VERSION,
                         static_cast<uint32_t>(relocatable_size),                 // m_textSize
                         static_cast<uint32_t>(strings_start),                    // m_stringsOffset
                         static_cast<uint32_t>(blobs_size > 0 ? blobs_start : 0), // m_blobsOffset
                         static_cast<uint32_t>(num_entries),                      // m_numEntries
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
                case ProgramBinaryElement::StructPointerSlot::Kind::Blob:
                    absolute = blobs_start + slot.blob_offset;
                    break;

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
        // 5. BLOB POOL
        // ========================================
        if (blobs_size > 0) {
            element.push_blob(blob_pool_bytes.data(), blobs_size,
                              /*relocation_bit=*/0);
        }
        for (u64 i = 0; i < blobs_pad; ++i) {
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

    void FileNode::add_import(FileNode *file) { m_imports.push_back(file); }

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