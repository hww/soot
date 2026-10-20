// DCScript.hpp

#pragma once

#include "CommonTypes.hpp"
#include "fmt/format.h"
#include "lib/StringId.hpp"
#include "lib/Variant.hpp"
#include "vm/Instructions.hpp"
#include <cstddef>
#include <string>
#include "CommonTypes.hpp"

// Thanks to icemesh

namespace carbon {

    // Well-known SIDs used across the DC format.
    constexpr sid64 SCRIPT_LAMBDA_SID = SID("script-lambda");
    constexpr sid64 ARRAY_SID = SID("array");
    constexpr sid64 GLOBAL_SID = SID("global");
    constexpr sid64 FUNCTION_SID = SID("function");
    constexpr sid64 SS_TYPE_SID = SID("ss-type");
    constexpr u64   DEADBEEF = 0xDEAD'BEEF'1337'F00D;

    struct SsDeclarationList;
    struct SsDeclaration;
    struct StateScript;
    struct SsOptions;
    struct SymbolArray;
    struct SsState;
    struct SsTrackGroup;
    struct SsOnBlock;
    struct SsTrack;
    struct SsLambda;
    struct ScriptLambda;
    struct SsType;

    // ---------------------------------------------------------------------------
    // Tracks (animation / audio / VFX lanes)
    // ---------------------------------------------------------------------------

    /// @brief Group of parallel tracks belonging to one SsOnBlock.
    struct SsTrackGroup {
        u64         m_always0;          ///< <c>0x00</c>: reserved, always 0
        u16         m_totalLambdaCount; ///< <c>0x08</c>: total number of lambdas across all tracks
        i16         m_numTracks;        ///< <c>0x0A</c>: number of tracks in m_aTracks
        u32         m_padding;          ///< <c>0x0C</c>: reserved, always 0
        SsTrack    *m_aTracks;          ///< <c>0x10</c>: pointer to SsTrack[m_numTracks]
        const char *m_name; ///< <c>0x18</c>: debug name, e.g. "ss-fp-test initial (on (start))"
        u64         m_always0_1;                ///< <c>0x20</c>: reserved, always 0
        u64         m_always0_2;                ///< <c>0x28</c>: reserved, always 0
        const ScriptLambda *m_rareScriptLambda; ///< <c>0x30</c>: rarely used script-lambda, or null
    };

    static_assert(sizeof(SsTrackGroup) == 0x38, "SsTrackGroup must be 0x38 bytes");

    // ---------------------------------------------------------------------------
    // States and transitions
    // ---------------------------------------------------------------------------

    /// @brief Block type of an SsOnBlock.
    /// @details Start/End/Event/Update/Virtual are canonical (see icemesh).
    ///          Code/Exit/Post are extensions observed in some state scripts.
    enum class BlockType : i32 {
        Enter,
        End,
        Event,
        Update,
        Virtual,
        Code, ///< (extension) code block
        Exit, ///< (extension) exit block
        Post  ///< (extension) post block
    };

    /// @brief One state of a state-script finite state machine.
    /// @example state idle { on (start) { ... } on (damage) { ... } }
    struct SsState // 0x18
    {
        sid64      m_stateId;       ///< <c>0x00</c>: SID64 of the state name
        i64        m_numSsOnBlocks; ///< <c>0x08</c>: number of SsOnBlock records
        SsOnBlock *m_pSsOnBlocks;   ///< <c>0x10</c>: pointer to SsOnBlock[m_numSsOnBlocks]

        /// @deprecated Kept for back-compatibility; always returns false.
        bool is_virtual() { return false; }
        /// @deprecated Kept for back-compatibility; always returns false.
        bool is_override() { return false; }

        StringId    state_id() const { return StringId(m_stateId); }
        std::string name() const { return StringId(m_stateId).to_string(); }

        SsOnBlock *lookup(BlockType type);
        SsOnBlock *lookup(BlockType type, StringId event_id);

        ScriptLambda *get_trans_function();
        ScriptLambda *get_enter_function();
        ScriptLambda *get_exit_function();
        ScriptLambda *get_post_function();
        ScriptLambda *get_code_function();
        ScriptLambda *get_event_handler(StringId event_id);
    };

    static_assert(sizeof(SsState) == 0x18, "SsState must be 0x18 bytes");

    inline const char *block_type_to_string(BlockType type) {
        switch (type) {
        case BlockType::Enter: return "enter";
        case BlockType::End: return "end";
        case BlockType::Event: return "event";
        case BlockType::Update: return "update";
        case BlockType::Virtual: return "virtual";
        case BlockType::Code: return "code";
        case BlockType::Exit: return "exit";
        case BlockType::Post: return "post";
        }
        return "unknown";
    }

    /// @brief Event-handler block inside a state.
    /// @example (on (start) (lambda () (print "entered idle")))
    struct SsOnBlock // 0x40 (plus SsTrackGroup)
    {
        BlockType m_blockType; ///< <c>0x00</c>: block type (start / end / event / update / virtual
                               ///< / code / exit / post)
        u32       m_always0;   ///< <c>0x04</c>: reserved, always 0
        sid64     m_blockEventId; ///< <c>0x08</c>: SID64 of the event; 0 if not an Event block
        ScriptLambda
            *m_pScriptLambda;      ///< <c>0x10</c>: pointer to the block's script-lambda, or null
        SsTrackGroup m_trackGroup; ///< <c>0x18</c>: parallel track group attached to this block

        ScriptLambda *lambda() { return m_pScriptLambda; }

        std::string name() const {
            if (m_blockType != BlockType::Event) { return block_type_to_string(m_blockType); }
            return fmt::format("{} {}", block_type_to_string(m_blockType),
                               StringId(m_blockEventId));
        }
    };

    static_assert(sizeof(SsOnBlock) == 0x50, "SsOnBlock must be 0x50 bytes");

    // ---------------------------------------------------------------------------
    // State script
    // ---------------------------------------------------------------------------
    /// @brief State script — a finite-state machine used for gameplay logic.
    /// @example (state-script npc-ai (initial idle) ...)
    struct StateScript // 0x50
    {
        sid64 m_stateScriptId;        ///< 0x00
        SsType     *m_pVarType;       ///< 0x08: раскладка переменных автомата (relocated)
        sid64       m_initialStateId; ///< 0x10
        SsOptions  *m_pSsOptions;     ///< 0x18 (relocated)
        void       *m_pVarDefaults;   ///< 0x20: экземпляр со значениями по умолчанию (relocated, blob pool)
        SsState    *m_pSsStateTable;  ///< 0x28 (relocated)
        i16         m_stateCount;     ///< 0x30
        i16         m_line;           ///< 0x32
        u32         m_always0_2;      ///< 0x34
        const char *m_pDebugFileName; ///< 0x38 (relocated, string pool)
        const char *m_pErrorName;     ///< 0x40 (relocated, string pool)
        u64         m_padding;        ///< 0x48
    
          

        inline SsState *resolve_state(StringId state_id) {
            for (i16 i = 0; i < m_stateCount; ++i) {
                if (m_pSsStateTable[i].state_id() == state_id) { return &m_pSsStateTable[i]; }
            }
            return nullptr;
        }
    
    };                                // 0x50



    static_assert(sizeof(StateScript) == 0x50, "StateScript must be 0x50 bytes");


    // ---------------------------------------------------------------------------
    // State-script declarations
    // ---------------------------------------------------------------------------
    
    /// @brief List of variables declared in a state script.
    struct SsDeclarationList // 0x10
    {
        u32 m_totalDeclarationSize;     ///< <c>0x00</c>: total size in bytes of all declared variables
        u32 m_numDeclarations;          ///< <c>0x04</c>: number of entries in m_pDeclarations
        SsDeclaration *m_pDeclarations; ///< <c>0x08</c>: pointer to SsDeclaration[m_numDeclarations]
    };

    static_assert(sizeof(SsDeclarationList) == 0x10, "SsDeclarationList must be 0x10 bytes");

    /// @brief One variable declared in a state script.
    /// @example (define health 100)  →  health: int32 = 100
    /// @details m_declTypeId selects how to interpret m_pDeclValue. Known types:
    ///            "boolean"      1 byte   (u8, printed as true/false)
    ///            "int32"        4 bytes  (i32)
    ///            "uint64"       8 bytes  (u64)
    ///            "float"        4 bytes  (f32)
    ///            "timer"        4 bytes  (f32)
    ///            "bound-frame"  4 bytes  (f32)
    ///            "symbol"       8 bytes  (sid64)
    ///            "string"       8 bytes  (const char*, relocated)
    ///            "vector"      16 bytes  (4 x f32)
    ///            "quat"        16 bytes  (4 x f32)
    ///            "point"       12 bytes  (3 x f32)
    ///          Unknown types must be inspected manually — the inspector prints
    ///          the first 16 bytes as raw hex when the type is not recognised.
    ///          See common/carbon/file/DeclarationTypes.hpp for the authoritative list.
    struct SsDeclaration // 0x30
    {
        sid64 m_declId;               ///< <c>0x00</c>: SID64 of the variable name (e.g. SID("#health"))
        const char *m_declIdString;   ///< <c>0x08</c>: source-string of the variable name (debug), or null
        sid64 m_declTypeId;           ///< <c>0x10</c>: SID64 of the type; see the type table above
        u16   m_varSizeSum;           ///< <c>0x18</c>: cumulative size in bytes of all declarations up to and
                                      ///< including this one
        u16   m_isVar;                ///< <c>0x1A</c>: 1 if this is a variable, 0 otherwise
        u32   m_always0;              ///< <c>0x1C</c>: reserved, always 0
        void *m_pDeclValue;           ///< <c>0x20</c>: pointer to the initial value in the data segment, or
                                      ///< null
        u64   m_always0x80;           ///< <c>0x28</c>: reserved; observed as 0x80 in practice
    };

    static_assert(sizeof(SsDeclaration) == 0x30, "SsDeclaration must be 0x30 bytes");

    // ---------------------------------------------------------------------------
    // State-script options
    // ---------------------------------------------------------------------------
    
    /// @brief Options block attached to a state script.
    struct SsOptions // 0x50
    {
        const char  *m_optionString; ///< <c>0x00</c>: debug string of options, or null
        u64          m_unknownFlags; ///< <c>0x08</c>: unknown flags; purpose not confirmed
        u64          m_always0_1;    ///< <c>0x10</c>: reserved, always 0
        SymbolArray *m_pSymbolArray; ///< <c>0x18</c>: primary symbol array (the options list)
        SymbolArray *m_symbolArray2; ///< <c>0x20</c>: secondary symbol array, overwhelmingly null
                                     ///< in practice
        SymbolArray *
            m_symbolArray3; ///< <c>0x28</c>: tertiary symbol array, overwhelmingly null in practice
        SymbolArray *m_symbolArray4; ///< <c>0x30</c>: quaternary symbol array, overwhelmingly null
                                     ///< in practice
        u32          m_always5;      ///< <c>0x38</c>: unknown, observed as 5
        u32          m_mostly0;      ///< <c>0x3C</c>: usually 0; occasionally 16, 32, 1, 13, 21, or 8
        u64          m_mostly0Rarely1; ///< <c>0x40</c>: usually 0, rarely 1
        u64          m_always0_2;    ///< <c>0x48</c>: reserved, always 0
    };
    
    static_assert(sizeof(SsOptions) == 0x50, "SsOptions must be 0x50 bytes");

    /// @brief Array of SID64 values (e.g. the options list of a state script).
    struct SymbolArray // 0x10
    {
        u32    m_numEntries; ///< <c>0x00</c>: number of entries in m_pSymbols
        u32    m_unk;        ///< <c>0x04</c>: reserved, always 0
        sid64 *m_pSymbols;   ///< <c>0x08</c>: pointer to sid64[m_numEntries]
    };

    static_assert(sizeof(SymbolArray) == 0x10, "SymbolArray must be 0x10 bytes");

    // ---------------------------------------------------------------------------
    // Tracks (animation / audio / VFX lanes)
    // ---------------------------------------------------------------------------

    /// @brief One track (animation / audio / VFX lane) inside an SsTrackGroup.
    struct SsTrack // 0x18
    {
        sid64     m_trackId;          ///< <c>0x00</c>: SID64 of the track name
        u16       m_trackIdx;         ///< <c>0x08</c>: index of this track inside its SsTrackGroup
        i16       m_totalLambdaCount; ///< <c>0x0A</c>: number of lambdas in m_pSsLambda
        u32       m_padding;          ///< <c>0x0C</c>: reserved, always 0
        SsLambda *m_pSsLambda;        ///< <c>0x10</c>: pointer to SsLambda[m_totalLambdaCount]
    };

    static_assert(sizeof(SsTrack) == 0x18, "SsTrack must be 0x18 bytes");

    /// @brief One lambda entry inside an SsTrack.
    struct SsLambda // 0x10
    {
        ScriptLambda *m_pScriptLambda; ///< <c>0x00</c>: pointer to the lambda body
        u64 m_someSortOfCounter; ///< <c>0x08</c>: monotonically increasing counter across the file;
                                 ///< may have gaps
    };

    static_assert(sizeof(SsLambda) == 0x10, "SsLambda must be 0x10 bytes");

    // ---------------------------------------------------------------------------
    // SsType — self-describing type information stored inside the .bin file.
    // ---------------------------------------------------------------------------
    //
    // A SsType entry carries the full layout of a user-defined type so that
    // the loader can interpret data-instance entries without a C++ header.
    // It is emitted by TypeDeclarationNode::generate and is referenced by
    // name from data-instance entries (via DCEntry::m_typeId).
    //
    // Layout:
    //   0x00  m_name        SID64 of the type name (e.g. SID("vec4"))
    //   0x08  m_parent      SID64 of the parent type (0 if none)
    //   0x10  m_size        size of an instance in bytes
    //   0x14  m_align       alignment of an instance in bytes
    //   0x18  m_numFields   number of entries in m_pFields
    //   0x1C  m_numMethods  number of entries in m_pMethods (0 for now)
    //   0x20  m_pFields     pointer to SsField[m_numFields]  (relocated)
    //   0x28  m_pMethods    pointer to SsMethod[m_numMethods] (relocated, may be null)
    //   0x30  m_flags       bit 0: is_basic (has runtime type info)
    //                       bit 1: is_structure (no runtime type info)
    //   0x38  m_reserved    reserved, always 0

    struct SsField // 0x28
    {
        sid64 m_name; ///< 0x00: SID64 имени поля
        const char *m_nameString; ///< 0x08: ptr на исходную строку имени в string pool (debug)
        sid64 m_type;   ///< 0x10: SID64 типа данных
        u32   m_offset; ///< 0x18: байт-офсет поля внутри инстанса
        u32   m_size;   ///< 0x1C: размер поля в байтах
        u32   m_flags;  ///< 0x20: bit 0: inline, bit 1: dynamic, bit 2: array, bit 3: is_const
        u32   m_count;  ///< 0x24: кол-во элементов, если array
    };                  // 0x28

    static_assert(sizeof(SsField) == 0x28, "SsField must be 0x28 bytes");

    struct SsType // 0x40
    {
        sid64    m_name;       ///< <c>0x00</c>: SID64 of the type name
        sid64    m_parent;     ///< <c>0x08</c>: SID64 of the parent type, 0 if none
        u32      m_size;       ///< <c>0x10</c>: size of an instance in bytes
        u32      m_align;      ///< <c>0x14</c>: alignment of an instance in bytes
        u32      m_numFields;  ///< <c>0x18</c>: number of fields
        u32      m_numMethods; ///< <c>0x1C</c>: number of methods (0 for now)
        SsField *m_pFields;    ///< <c>0x20</c>: pointer to SsField[m_numFields] (relocated)
        void    *m_pMethods;   ///< <c>0x28</c>: reserved for SsMethod[] (relocated, may be null)
        u64      m_flags;      ///< 0x30: bit 0: basic, bit 1: structure, bit 2: is_state_vars
        u64      m_reserved;   ///< <c>0x38</c>: reserved, always 0
    };

    static_assert(sizeof(SsType)  == 0x40, "SsType must be 0x40 bytes");

    // ---------------------------------------------------------------------------
    // SsMethod — one entry in an SsType's method VTable.
    // ---------------------------------------------------------------------------
    //
    // The method ID is the index of this entry in the SsMethod[] array. Slots are
    // ordered by ascending method ID:
    //   - The first N slots correspond to built-in methods (object::new, delete,
    //     print, ...). They are always present with m_pLambda == nullptr; their
    //     implementations live in C++ (TypeSystem::add_builtin_types).
    //   - User-defined methods (from deftype :methods and defmethod) follow, in
    //     the order they were declared. Each carries a pointer to its ScriptLambda.
    //
    // The array is sized to (highest method ID + 1), so a call-by-id can index
    // it directly. Unimplemented methods keep their slot with m_pLambda == nullptr.
    //
    // Methods may be overridden and may change signature: the slot is looked up
    // by ID through TypeSystem, so a child type's method simply replaces the
    // parent's in its own VTable.
    struct SsMethod // 0x10
    {
        sid64         m_name;    ///< <c>0x00</c>: SID of the method name (e.g. SID("len"))
        sid64         m_fullName;///< <c>0x08</c>: SID of the full method name (e.g. SID("vec3-len"))
        ScriptLambda *m_pLambda; ///< <c>0x08</c>: pointer to the implementation (relocated),
                                 ///< or null for built-in / unimplemented methods
    };

    static_assert(sizeof(SsMethod) == 0x18, "SsMethod must be 0x18 bytes");

    // ---------------------------------------------------------------------------
    // Executable function (lambda)
    // ---------------------------------------------------------------------------

    using lambda_symbol_entry = u64; ///< One 64-bit entry in a ScriptLambda symbol table.

    /// @brief Compiled function / lambda header. One per entry of type SID("script-lambda").
    /// @details Layout (0x50 bytes total):
    ///            <c>0x00</c>  m_type             type-slot SID
    ///            <c>0x08</c>  m_pInstruction     pointer to Instruction[m_numInstructions]
    ///            <c>0x10</c>  m_pSymbols         pointer to the symbol table (u64[m_numSymbols])
    ///            <c>0x18</c>  m_typeId           SID("script-lambda") in the code generator
    ///            <c>0x20</c>  m_sum              size reported by get_scriptlambda_sum()
    ///            <c>0x28</c>  m_funcName         SID of the function name; 0 for embedded lambdas
    ///            <c>0x30</c>  m_instructionFlag  magic 0xDEADBEEF1337F00D
    ///            <c>0x38</c>  m_always0_2        reserved, always 0
    ///            <c>0x3C</c>  m_numInstructions  number of instructions
    ///            <c>0x40</c>  m_numSymbols       number of symbol-table entries (notcanonical:
    ///            skipped
    ///                                             in the aggregate initializer in function.cpp)
    ///            <c>0x44</c>  m_neg              always -1
    ///            <c>0x48</c>  m_sidGlobal        SID("global") for top-level, else scope SID
    ///            <c>0x50</c>  m_always0_3        reserved, always 0
    struct ScriptLambda // 0x58
    {
        sid64 m_type;           ///< <c>0x00</c>: type-slot SID (notcanonical: doc suggests a value-type SID
                                ///< such as SID("vector3"),
                                ///<                  but the code generator writes SID("script-lambda") here)
        u64  *m_pInstruction;   ///< <c>0x08</c>: pointer to Instruction[m_numInstructions]
        u64  *m_pSymbols;       ///< <c>0x10</c>: pointer to the symbol table (u64[m_numSymbols])
        sid64 m_typeId;         ///< <c>0x18</c>: container type SID; the code generator writes
                                ///< SID("script-lambda") here
        u64   m_sum;            ///< <c>0x20</c>: size in bytes; function::get_scriptlambda_sum() returns 12 +
                                ///< 4*(numInstr + numSymbols)
        sid64 m_funcName;       ///< <c>0x28</c>: SID of the function name; 0 for embedded (anonymous)
                                ///< lambdas
        u64 m_instructionFlag;  ///< <c>0x30</c>: magic marker 0xDEADBEEF1337F00D, identifies a valid
                                ///< ScriptLambda
        u32 m_always0_2;        ///< <c>0x38</c>: reserved, always 0
        u32 m_numInstructions;  ///< <c>0x3C</c>: number of instructions in m_pInstruction
        u32 m_numSymbols;       ///< <c>0x40</c>: number of entries in m_pSymbols (notcanonical: this
                                ///< field is skipped
                                ///<                  in the aggregate initializer in function.cpp, so in
                                ///<                  practice the value is unreliable and must be derived
                                ///<                  from m_sum)
        i32 m_neg;              ///< <c>0x44</c>: always -1; end-marker/signature
        u64 m_sidGlobal;        ///< <c>0x48</c>: SID("global") for top-level functions, otherwise the
                                ///< scope SID
                                ///<                  (e.g. state-script/lambda)
        u64 m_always0_3;        ///< <c>0x50</c>: reserved, always 0

        Instruction *get_code_ptr() const {
            return reinterpret_cast<Instruction *>(m_pInstruction);
        }
        u64 *get_symbols_ptr() const { return m_pSymbols; }
    };

    // ScriptLambda is 0x58 in the carbon format (two extra fields not present
    // in the icemesh reference layout, where it is 0x50).
    static_assert(sizeof(ScriptLambda) == 0x58, "ScriptLambda must be 0x58 bytes");

    inline SsOnBlock *SsState::lookup(BlockType type) {
        for (i64 i = 0; i < m_numSsOnBlocks; ++i) {
            if (m_pSsOnBlocks[i].m_blockType == type) { return &m_pSsOnBlocks[i]; }
        }
        return nullptr;
    }

    inline SsOnBlock *SsState::lookup(BlockType type, StringId event_id) {
        for (i64 i = 0; i < m_numSsOnBlocks; ++i) {
            const SsOnBlock &block = m_pSsOnBlocks[i];
            if (block.m_blockType == type && block.m_blockEventId == event_id) {
                return &m_pSsOnBlocks[i];
            }
        }
        return nullptr;
    }

    inline ScriptLambda *SsState::get_trans_function() {
        const auto *e = lookup(BlockType::Update);
        return e ? e->m_pScriptLambda : nullptr;
    }
    inline ScriptLambda *SsState::get_enter_function() {
        const auto *e = lookup(BlockType::Enter);
        return e ? e->m_pScriptLambda : nullptr;
    }
    inline ScriptLambda *SsState::get_exit_function() {
        const auto *e = lookup(BlockType::Exit);
        return e ? e->m_pScriptLambda : nullptr;
    }
    inline ScriptLambda *SsState::get_post_function() {
        const auto *e = lookup(BlockType::Post);
        return e ? e->m_pScriptLambda : nullptr;
    }
    inline ScriptLambda *SsState::get_code_function() {
        const auto *e = lookup(BlockType::Code);
        return e ? e->m_pScriptLambda : nullptr;
    }
    inline ScriptLambda *SsState::get_event_handler(StringId event_id) {
        const auto *e = lookup(BlockType::Event, event_id);
        return e ? e->m_pScriptLambda : nullptr;
    }


    // ---------------------------------------------------------------------------
    // On-disk representation of `map` (used by BinaryFileInspector).
    // ---------------------------------------------------------------------------
 
    // On-disk representation of `map`.
    //
    // DcMap:
    //   m_typeId   = SID("map") or SID("map-32")
    //   m_size     = number of elements
    //   m_pKeys    = pointer to sid64[m_size]   (relocated)
    //   m_pValues  = pointer to u64[m_size]     (relocated)
    //
    // Each m_pValues[i] points at the `m_data` field of a DcMapValue.
    // The full value is obtained by stepping back 8 bytes:
    //
    //     const DcMapValue *v =
    //         reinterpret_cast<const DcMapValue*>(m_pValues[i] - 8);
    //
    // This mirrors Disassembler::insert_struct, case SID("map"), which does
    // exactly the same `- 8` adjustment.
    //
    struct DcMap {
        sid64  m_typeId;
        u64    m_size;
        sid64 *m_pKeys;
        u64   *m_pValues;
    };

    // The typed wrapper around a single map value. Equivalent to the game's
    // structs::unmapped, reproduced here so that libcarbon does not depend
    // on the disassembler's namespace.
    struct DcMapValue {
        sid64 m_typeId; // +0x00: SID of the value type
        void *m_data;   // +0x08: pointer to the payload
    };

    static_assert(sizeof(DcMap) == 0x20, "DcMap must be 0x20 bytes");
    static_assert(sizeof(DcMapValue) == 0x10, "DcMapValue must be 0x10 bytes");

    // ---------------------------------------------------------------------------
    // DcArray — standalone array on disk.
    // ---------------------------------------------------------------------------
    //
    // Homogeneous array: one element type, one count, one data pointer.
    // The game's own format has no self-describing array: an array there is
    // just a u64* with the type and count living in the parent struct. We
    // gather those three fields into one place so that our own files can
    // carry arrays as first-class values, symmetric with DcMap.
    //
    // Layout:
    //   +0x00  sid64  m_typeId    SID of the element type
    //   +0x08  u64    m_size      number of elements
    //   +0x10  u64*   m_pValues   pointer to the element block (relocated)
    //
    // How to read the elements:
    //
    //   scalar type (int32, f32, sid64, ...)
    //       m_pValues points to a flat block of scalars. m_pValues[i] is
    //       the i-th element, reinterpreted as the concrete type.
    //
    //   reference type (structure, cstr, ...)
    //       m_pValues[i] points at the payload of a DcMapValue. The full
    //       wrapper is obtained by stepping back 8 bytes, exactly as in
    //       DcMap:
    //
    //           const DcMapValue *v =
    //               reinterpret_cast<const DcMapValue*>(m_pValues[i] - 8);
    //
    struct DcArray {
        sid64 m_typeId;
        u64   m_size;
        u64  *m_pValues;
    };

    static_assert(sizeof(DcArray) == 0x18, "DcArray must be 0x18 bytes");
}

