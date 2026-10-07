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
    ProgramBinaryElement FileNode::generate(GlobalState &state) {
        auto entries = collect_all(state);

        if (entries.empty()) {
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
        auto                         element = make_binary(std::move(entries), state, data_structs);
        m_dataStructs = std::move(data_structs);
        return element;
    }

    // ============================================================================
    // collect_all - собирает ProgramBinaryElement для всех функций
    // ============================================================================
    std::vector<ProgramBinaryElement> FileNode::collect_all(GlobalState &state) {
        std::vector<ProgramBinaryElement> entries;

        for (auto &child : m_children) {
            if (auto *fn = dynamic_cast<FunctionNode *>(child.get())) {
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
                ss_methods[id] = {0, nullptr};
                continue;
            }

            SsMethod &sm = ss_methods[id];
            sm.m_name = StringId(info.name).value;

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
                                               GlobalState                      &state,
                                               std::vector<DataStructEntry>     &out_data_structs) {
        printf("=== make_binary DEBUG ===\n");
        printf("program_elements.size() = %zu\n", program_elements.size());

        if (program_elements.empty()) { return ProgramBinaryElement(0); }

        constexpr sid64 ARRAY_SID = SID("array");
        constexpr u64   first_entry_offset = 0x28;
        constexpr u32   header_size = sizeof(DC_Header) + sizeof(ARRAY_SID);

        // ------------------------------------------------------------------
        // Pad every program element's raw data to a multiple of 8 bytes, so
        // that concatenating them into element.m_rawData keeps slot boundaries
        // aligned.
        // ------------------------------------------------------------------
        for (auto &el : program_elements) {
            while (el.m_rawData.size() % 8 != 0) { el.m_rawData.push_back(std::byte{0}); }
            const size_t slots = (el.m_rawData.size() + 7) / 8;
            while (el.m_relocTable.size() < slots) { el.m_relocTable.push_back(false); }
            el.check_size();
        }

        // ------------------------------------------------------------------
        // Pass 1: compute file offsets for method lambdas.
        // ------------------------------------------------------------------
        // One method_offsets map per type declaration. SsType elements are
        // built AFTER this pass, when all offsets are known.
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

        // Total entry count = existing program elements + SsType elements.
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

        // ------------------------------------------------------------------
        // Build SsType elements with the real method offsets, then append.
        // ------------------------------------------------------------------
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

        const u64 stringtable_size =
            std::accumulate(state.m_strings.begin(), state.m_strings.end(), u64{0},
                            [](u64 acc, const std::string &s) { return acc + s.size() + 1; });

        std::vector<char> stringtable;
        stringtable.reserve(stringtable_size);
        for (const auto &s : state.m_strings) {
            stringtable.insert(stringtable.end(), s.begin(), s.end());
            stringtable.push_back('\0');
        }

        const u64 data_size = header_size + entries_size + entries_data_size;
        const u64 total_size =
            data_size + stringtable_size + 4 + ((data_size + stringtable_size + 63) / 64);

        printf("total_size = %lu\n", (unsigned long)total_size);

        ProgramBinaryElement element(total_size);

        // ========================================
        // 1. HEADER
        // ========================================
        DC_Header header{DC_FILE_MAGIC,
                         DC_FILE_VERSION,
                         static_cast<uint32_t>(data_size + stringtable_size),
                         static_cast<uint32_t>(data_size),
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
            lg::info("FileNode::make_binary entry {}", entry.to_string());

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
        for (auto &fn : program_elements) {
            for (const auto offset : fn.m_stringOffsets) {
                const u64 str_index = *reinterpret_cast<u64 *>(&fn.m_rawData[offset]);
                u64       relative_offset = data_size;
                for (u32 i = 0; i < str_index; ++i) {
                    relative_offset += state.m_strings[i].size() + 1;
                }
                *reinterpret_cast<u64 *>(&fn.m_rawData[offset]) =
                    relative_offset - element.m_rawData.size();
            }

            fn.adjust_offsets(element.m_rawData.size());

            element.m_rawData.insert(element.m_rawData.end(), fn.m_rawData.begin(),
                                     fn.m_rawData.end());

            for (size_t i = 0; i < fn.m_relocTable.size(); ++i) {
                element.m_relocTable.push_back(fn.m_relocTable[i]);
            }

            element.check_size();
        }

        // ========================================
        // 4. STRING TABLE
        // ========================================
        element.m_rawData.insert(
            element.m_rawData.end(), reinterpret_cast<const std::byte *>(stringtable.data()),
            reinterpret_cast<const std::byte *>(stringtable.data()) + stringtable.size());

        size_t padding = (4 - (stringtable.size() % 4)) % 4;
        element.m_rawData.insert(element.m_rawData.end(), padding, std::byte{0});

        const size_t reloc_bytes = (element.m_relocTable.size() + 7) / 8;
        const u32    reloc_size = static_cast<u32>(reloc_bytes);
        element.push_value(reloc_size);
        lg::info("Constuct reloc table with size {}", reloc_size);

        for (size_t i = 0; i < reloc_bytes; ++i) {
            uint8_t byte = 0;
            for (size_t bit = 0; bit < 8; ++bit) {
                size_t idx = i * 8 + bit;
                if (idx < element.m_relocTable.size() && element.m_relocTable[idx]) {
                    byte |= (1 << bit);
                }
            }
            element.push_value(byte);
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