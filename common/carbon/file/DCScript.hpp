#pragma once

#include "CommonTypes.hpp"
#include "fmt/format.h"
#include "lib/StringId.hpp"
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

    // ---------------------------------------------------------------------------
    // State script
    // ---------------------------------------------------------------------------
    /// @brief State script — a finite-state machine used for gameplay logic.
    /// @example (state-script npc-ai (initial idle) ...)
    struct StateScript // 0x50
    {
        sid64 m_stateScriptId;               ///< <c>0x08</c>: SID64 of the state-script name
        SsDeclarationList *m_pSsDeclList;    ///< <c>0x10</c>: pointer to the declaration list, or null
        sid64              m_initialStateId; ///< <c>0x18</c>: SID64 of the initial state
        SsOptions         *m_pSsOptions;     ///< <c>0x20</c>: pointer to the options block, or null
        u64                m_always0_1;      ///< <c>0x28</c>: reserved, always 0
        SsState           *m_pSsStateTable;  ///< <c>0x30</c>: pointer to the SsState array
        i16                m_stateCount;     ///< <c>0x38</c>: number of states in m_pSsStateTable
        i16                m_line;           ///< <c>0x3A</c>: source line number shown in the debug overlay
        u32                m_always0_2;      ///< <c>0x3C</c>: reserved, always 0
        const char        *m_pDebugFileName; ///< <c>0x40</c>: source path, e.g.
                                             ///< "t2r/src/game/scriptx/ss/ss-tag-as-hero.dcx"
        const char        *m_pErrorName;     ///< <c>0x48</c>: runtime error string, or null
        u64                m_padding;        ///< <c>0x50</c>: reserved, always 0

        SsState *resolve_state(StringId name);
    };


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
        sid64 m_declId;         ///< <c>0x00</c>: SID64 of the variable name (e.g. SID("#health"))
        const char
             *m_declIdString;   ///< <c>0x08</c>: source-string of the variable name (debug), or null
        sid64 m_declTypeId;     ///< <c>0x10</c>: SID64 of the type; see the type table above
        u16   m_varSizeSum;     ///< <c>0x18</c>: cumulative size in bytes of all declarations up to and
                                ///< including this one
        u16   m_isVar;          ///< <c>0x1A</c>: 1 if this is a variable, 0 otherwise
        u32   m_always0;        ///< <c>0x1C</c>: reserved, always 0
        void *m_pDeclValue;     ///< <c>0x20</c>: pointer to the initial value in the data segment, or
                                ///< null
        u64   m_always0x80;     ///< <c>0x28</c>: reserved; observed as 0x80 in practice
    };

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

    /// @brief Array of SID64 values (e.g. the options list of a state script).
    struct SymbolArray // 0x10
    {
        u32    m_numEntries; ///< <c>0x00</c>: number of entries in m_pSymbols
        u32    m_unk;        ///< <c>0x04</c>: reserved, always 0
        sid64 *m_pSymbols;   ///< <c>0x08</c>: pointer to sid64[m_numEntries]
    };

    // ---------------------------------------------------------------------------
    // Tracks (animation / audio / VFX lanes)
    // ---------------------------------------------------------------------------
    
    /// @brief Group of parallel tracks belonging to one SsOnBlock.
    struct SsTrackGroup {
        u64         m_always0;                  ///< <c>0x00</c>: reserved, always 0
        u16         m_totalLambdaCount;         ///< <c>0x08</c>: total number of lambdas across all tracks
        i16         m_numTracks;                ///< <c>0x0A</c>: number of tracks in m_aTracks
        u32         m_padding;                  ///< <c>0x0C</c>: reserved, always 0
        SsTrack    *m_aTracks;                  ///< <c>0x10</c>: pointer to SsTrack[m_numTracks]
        const char *m_name;                     ///< <c>0x18</c>: debug name, e.g. "ss-fp-test initial (on (start))"
        u64         m_always0_1;                ///< <c>0x20</c>: reserved, always 0
        u64         m_always0_2;                ///< <c>0x28</c>: reserved, always 0
        const ScriptLambda *m_rareScriptLambda; ///< <c>0x30</c>: rarely used script-lambda, or null
    };

    // ---------------------------------------------------------------------------
    // States and transitions
    // ---------------------------------------------------------------------------

    /// @brief Block type of an SsOnBlock.
    /// @details Start/End/Event/Update/Virtual are canonical (see icemesh).
    ///          Code/Exit/Post are extensions observed in some state scripts.
    enum class BlockType : i32 {
        Start,
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

    inline const char *block_type_to_string(BlockType type) {
        switch (type) {
        case BlockType::Start: return "start";
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
        BlockType     m_blockType;      ///< <c>0x00</c>: block type (start / end / event / update / virtual
                                        ///< / code / exit / post)
        u32           m_always0;        ///< <c>0x04</c>: reserved, always 0
        sid64         m_blockEventId;   ///< <c>0x08</c>: SID64 of the event; 0 if not an Event block
        ScriptLambda *m_pScriptLambda;  ///< <c>0x10</c>: pointer to the block's script-lambda, or null
        SsTrackGroup  m_trackGroup;     ///< <c>0x18</c>: parallel track group attached to this block

        ScriptLambda *lambda() { return m_pScriptLambda; }

        std::string name() const {
            if (m_blockType != BlockType::Event) { return block_type_to_string(m_blockType); }
            return fmt::format("{} {}", block_type_to_string(m_blockType),
                               StringId(m_blockEventId));
        }
    };

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

    /// @brief One lambda entry inside an SsTrack.
    struct SsLambda // 0x10
    {
        ScriptLambda *m_pScriptLambda; ///< <c>0x00</c>: pointer to the lambda body
        u64 m_someSortOfCounter; ///< <c>0x08</c>: monotonically increasing counter across the file;
                                 ///< may have gaps
    };

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
        const auto *e = lookup(BlockType::Start);
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

    inline SsState *StateScript::resolve_state(StringId state_id) {
        for (i16 i = 0; i < m_stateCount; ++i) {
            if (m_pSsStateTable[i].state_id() == state_id) { return &m_pSsStateTable[i]; }
        }
        return nullptr;
    }
}

