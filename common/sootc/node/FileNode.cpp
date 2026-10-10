// FileNode.cpp
#include "common/sootc/node/FileNode.hpp"
#include "DataDeclarationNode.hpp"
#include "EnumDeclarationNode.hpp"
#include "TypeDeclarationNode.hpp"
#include "common/carbon/file/DCHeader.hpp"
#include "common/sootc/node/FunctionNode.hpp"
#include "common/util/Log.hpp"
#include "sootc/compiler/CompilerError.hpp"
#include "sootc/node/DataDeclarationNode.hpp"

#include <cassert>
#include <cstring>
#include <numeric>
#include <type_system/TypeSystem.hpp>

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
    ProgramBinaryElement FileNode::generate(StringsTable &strings_table) {
        auto entries = collect_all(strings_table);

        // Even with no functions or data declarations, a file may still
        // need to emit SsType entries for its (deftype ...) forms. Only
        // bail out when there is truly nothing to emit.
        bool has_types = false;
        for (auto &child : m_children) {
            if (dynamic_cast<TypeDeclarationNode *>(child.get()) != nullptr) {
                has_types = true;
                break;
            }
        }

        if (entries.empty() && !has_types) {
            std::string types;
            for (auto &child : m_children) {
                if (!types.empty()) types += ", ";
                types += child->get_node_type_string();
            }
            lg::info("FileNode::generate: no emittable entries ({} children: [{}])",
                     m_children.size(), types.empty() ? "<none>" : types);
            return ProgramBinaryElement(0);
        }

        std::vector<DataStructEntry> data_structs;
        auto element = make_binary(std::move(entries), strings_table, data_structs);
        m_dataStructs = std::move(data_structs);
        return element;
    }

    // ============================================================================
    // collect_all - собирает ProgramBinaryElement для всех функций
    // ============================================================================
    std::vector<ProgramBinaryElement> FileNode::collect_all(StringsTable &state) {
        std::vector<ProgramBinaryElement> entries;

        for (auto &child : m_children) {
            if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
                fn->set_global_state(&state); // ← добавить ЭТУ строку
                fn->emit_body();
                entries.push_back(fn->generate(state));
                lg::info("Function '{}': {} instructions, {} constants", fn->name(),
                         fn->instructions().size(), fn->constants().size());
            } else if (auto *decl = dynamic_cast<DataDeclarationNode *>(child.get())) {
                ProgramBinaryElement element = decl->generate(state);
                const size_t         payload_size = element.m_rawData.size();
                if (payload_size > 0) {
                    entries.push_back(std::move(element));
                    lg::info("Data declaration '{}': {} bytes", decl->name(), payload_size);
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
                                               StringsTable                     &strings_table,
                                               std::vector<DataStructEntry>     &out_data_structs) {
        lg::info("make_binary: program_elements.size()={}", program_elements.size());
        for (auto &fn : program_elements) {
            lg::info("  fn '{}': rawData={}, relocTable={}, stringOffsets={}",
                     StringIdManager::instance().get_string(fn.m_entry.m_nameID),
                     fn.m_rawData.size(), fn.m_relocTable.size(), fn.m_stringOffsets.size());
        }

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

        // Nothing at all to emit: no functions, no data, no types.
        if (program_elements.empty() && type_infos.empty()) { return ProgramBinaryElement(0); }

        // ------------------------------------------------------------------
        // Pad every program element's raw data to a multiple of 8 bytes.
        // ------------------------------------------------------------------
        for (auto &el : program_elements) {
            while (el.m_rawData.size() % 8 != 0) { el.m_rawData.push_back(std::byte{0}); }
            const size_t slots = (el.m_rawData.size() + 7) / 8;
            while (el.m_relocTable.size() < slots) { el.m_relocTable.push_back(false); }
            el.check_size();
        }

        const u64 total_entries = program_elements.size() + type_infos.size();

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

        for (auto &ti : type_infos) {
            ProgramBinaryElement ss_el = build_ss_type(ti.decl, ti.method_offsets);
            if (!ss_el.m_rawData.empty()) { program_elements.push_back(std::move(ss_el)); }
        }

        // ------------------------------------------------------------------
        // Pass 2: final layout.
        // ------------------------------------------------------------------
        const u64 num_entries = program_elements.size();
        const u64 entries_size = sizeof(DCEntry) * num_entries;

        const u64 entries_data_size =
            std::accumulate(program_elements.begin(), program_elements.end(), u64{0},
                            [](u64 acc, const ProgramBinaryElement &element) {
                                return acc + element.m_rawData.size();
                            });

        const auto &strings_table_characters = strings_table.bytes();

        const u64 strings_size = strings_table_characters.size();
        const u64 data_size = header_size + entries_size + entries_data_size;

        // The string table is padded to a 4-byte boundary, and the
        // relocation table starts right after the padding. The bitmap
        // has one bit per 8-byte slot of the relocatable region,
        // rounded up to whole bytes.
        const u64 string_padding = (4 - (strings_size % 4)) % 4;
        const u64 relocatable_size = data_size + strings_size + string_padding;
        const u64 reloc_bytes = (relocatable_size + 7) / 8;
        const u64 total_size = relocatable_size + 4 + reloc_bytes;

        printf("total_size = %lu\n", (unsigned long)total_size);

        ProgramBinaryElement element(total_size);

        // ========================================
        // 1. HEADER
        // ========================================
        DC_Header header{
            DC_FILE_MAGIC,
            DC_FILE_VERSION,
            static_cast<uint32_t>(relocatable_size), // text size (data + strings + padding)
            static_cast<uint32_t>(data_size),        // strings offset
            0x1,
            static_cast<uint32_t>(num_entries),
            reinterpret_cast<DCEntry *>(first_entry_offset)};
        element.push_value_with_ptr(header, PTR_FIELD(DC_Header, m_pStartOfData));
        element.push_value(ARRAY_SID);

        // ========================================
        // 2. ENTRY TABLE
        // ========================================
        const u64 first_function_start = header_size + num_entries * sizeof(DCEntry);
        u64       prev_entry_size = 0;

        out_data_structs.clear();

        for (auto &fn : program_elements) {
            DCEntry entry = fn.m_entry;
            entry.m_entryPtr = reinterpret_cast<void *>(first_function_start + prev_entry_size);
            element.push_value_with_ptr(entry, PTR_FIELD(DCEntry, m_entryPtr));
            prev_entry_size += fn.m_rawData.size();
            lg::debug("FileNode::make_binary entry {}", entry.to_string());

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
        //
        // We also build the file-level relocation bitmap here. The DC
        // relocation table has exactly ONE BIT PER 8-BYTE SLOT of the
        // whole file, so we cannot simply concatenate per-function bit
        // vectors: a bit at index N means "the u64 at file offset N*8 is
        // a pointer that must be relocated by the loader".
        //
        // For every function we know its absolute start offset in the
        // file (element.m_rawData.size() before we append its payload).
        // Each per-function reloc bit therefore maps to the file-level
        // bit at (function_start / 8) + per_function_bit.
        //
        // IMPORTANT: every write into element.m_rawData must go through
        // push_blob / push_value / push_value_with_ptr, or the
        // ProgramBinaryElement invariant (relocTable.size() ==
        // (rawData.size() + 7) / 8) is broken and check_size() throws.
        std::vector<bool> file_reloc_table(relocatable_size, false);

        for (auto &fn : program_elements) {
            // Patch string-constant slots: replace the string index with
            // the absolute file offset of the string in the global string
            // table, and mark the slot as relocatable so the loader turns
            // it into an absolute pointer at load time.
            //
            // The absolute offset is data_size (start of the string table
            // in the file) plus the string's offset inside the table.
            for (const auto &slot : fn.m_stringConstantSlots) {
                // slot.str_index is already the byte offset of the string
                // inside the string table (returned by lookup_or_add), not
                // an index into offsets(). So the absolute file offset is
                // simply data_size + that offset.
                const u32 str_offset_in_table = slot.str_offset_in_table;
                const u64 absolute_offset = data_size + str_offset_in_table;

                *reinterpret_cast<u64 *>(&fn.m_rawData[slot.slot_offset]) = absolute_offset;

                lg::debug("make_binary: string slot at payload+0x{:X} -> abs 0x{:X} "
                         "(str_index={}, table_offset=0x{:X})",
                         slot.slot_offset, absolute_offset, slot.str_offset_in_table,
                         str_offset_in_table);
            }

            // The string slots already contain absolute file offsets, so
            // adjust_offsets() must NOT add function_start to them. But they
            // still need to be marked in the final bitmap so the loader
            // converts them to absolute pointers. Temporarily clear their
            // bits for adjust_offsets, then restore them.
            std::vector<bool> saved_string_bits(fn.m_stringConstantSlots.size(), false);
            for (size_t i = 0; i < fn.m_stringConstantSlots.size(); ++i) {
                const size_t slot_bit = fn.m_stringConstantSlots[i].slot_offset / 8;
                if (slot_bit < fn.m_relocTable.size()) {
                    saved_string_bits[i] = fn.m_relocTable[slot_bit];
                    fn.m_relocTable[slot_bit] = false;
                }
            }

            const u64 function_start = element.m_rawData.size();

            fn.adjust_offsets(function_start);

            for (size_t i = 0; i < fn.m_stringConstantSlots.size(); ++i) {
                const size_t slot_bit = fn.m_stringConstantSlots[i].slot_offset / 8;
                if (slot_bit < fn.m_relocTable.size() && saved_string_bits[i]) {
                    fn.m_relocTable[slot_bit] = true;
                }
            }

            // Append the payload through push_blob so that element's
            // relocation bitmap grows by exactly ceil(fn.m_rawData.size() / 8)
            // new zero bits. We then overwrite the relevant bits with the
            // function's own reloc bits.
            element.push_blob(fn.m_rawData.data(), fn.m_rawData.size(), /*relocation_bit=*/0);

            // Fold this function's per-slot reloc bits into element's
            // bitmap. fn.m_relocTable[i] corresponds to the u64 slot at
            // (function_start / 8) + i in the file.
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
        // 4. STRING TABLE
        // ========================================
        //
        // Use push_blob so the invariant relocTable.size() ==
        // (rawData.size() + 7) / 8 keeps holding. Strings are never
        // pointers, so the relocation_bit is 0.
        if (strings_size > 0) {
            element.push_blob(strings_table_characters.data(), strings_size,
                              /*relocation_bit=*/0);
        }

        const size_t padding = (4 - (strings_size % 4)) % 4;
        if (string_padding > 0) {
            const std::byte zero = std::byte{0};
            for (u64 i = 0; i < string_padding; ++i) {
                element.push_blob(&zero, 1, /*relocation_bit=*/0);
            }
        }

        // ========================================
        // 5. RELOCATION TABLE
        // ========================================
        // Before writing the bitmap, copy every relocatable bit from the
        // in-memory element into file_reloc_table. push_value_with_ptr()
        // already marked the header (m_pStartOfData), each DCEntry
        // (m_entryPtr), and any other pointers; those bits live in
        // element.m_relocTable and must be preserved in the output bitmap.
        for (size_t i = 0; i < element.m_relocTable.size() && i < file_reloc_table.size(); ++i) {
            if (element.m_relocTable[i]) { file_reloc_table[i] = true; }
        }

        const u32 reloc_size = static_cast<u32>(reloc_bytes);

        element.push_value(reloc_size);
        lg::debug("Construct reloc table with size {}", reloc_size);

        for (u64 i = 0; i < reloc_bytes; ++i) {
            uint8_t byte = 0;
            for (size_t bit = 0; bit < 8; ++bit) {
                const u64 idx = i * 8 + bit;
                if (idx < file_reloc_table.size() && file_reloc_table[idx]) { byte |= (1 << bit); }
            }
            element.push_value(byte);
        }

        // Sanity check: the assembled element must be exactly total_size
        // bytes. If it is not, the header's text/string offsets won't match
        // the actual layout and BinaryFile::read_reloc_table will read past
        // the end of the buffer.
        //
        // Do NOT call element.check_size() here — the ProgramBinaryElement
        // invariant (relocTable.size() == (rawData.size() + 7) / 8) is
        // checked after every push_* call, and push_value for the reloc
        // table itself adds bits after the fact.
        if (element.m_rawData.size() != total_size) {
            throw std::runtime_error(
                fmt::format("FileNode::make_binary: assembled {} bytes but expected {}. "
                            "Header text/string offsets will be wrong.",
                            element.m_rawData.size(), total_size));
        }
        lg::debug("make_binary: assembled {} bytes (expected {})", element.m_rawData.size(),
                 total_size);

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