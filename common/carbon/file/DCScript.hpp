#pragma once
#include "fmt/format.h"
#include "lib/StringId.hpp"
#include "vm/Instructions.hpp"
#include <cstddef>
#include <string>
#ifndef DCSCRIPT_H
#define DCSCRIPT_H

// Thanks to icemesh

#include "CommonTypes.hpp"

using namespace carbon;


namespace carbon {

    // Константы из эталонного кода
    constexpr sid64 SCRIPT_LAMBDA_SID = SID("script-lambda");
    constexpr sid64 ARRAY_SID = SID("array");
    constexpr sid64 GLOBAL_SID = SID("global");
    constexpr sid64 FUNCTION_SID = SID("function");
    constexpr u64 DEADBEEF = 0xDEAD'BEEF'1337'F00D;

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

    // ============================================================================
    // STATE SCRIPT (конечный автомат)
    // ============================================================================

    /// @brief Главная структура State Script — конечного автомата для игровой логики
    /// @example (state-script npc-ai (initial idle) ...)
    struct StateScript //0x50
    {
        sid64               m_stateScriptId;     ///< <c>0x00</c>: StringId64 of the script name
        SsDeclarationList*  m_pSsDeclList;       ///< <c>0x08</c>: ptr to the declaration list
        sid64               m_initialStateId;    ///< <c>0x10</c>: StringId64 of the initial state
        SsOptions*          m_pSsOptions;        ///< <c>0x18</c>: ptr to the SsOptions
        u64                 m_always0_1;         ///< <c>0x20</c>: always 0 ?
        SsState*            m_pSsStateTable;     ///< <c>0x28</c>: ptr to the SsState Table
        i16                 m_stateCount;        ///< <c>0x30</c>: number of states
        i16                 m_line;              ///< <c>0x32</c>: this is a line number thats get displayed in the debug display
        u32                 m_always0_2;         ///< <c>0x34</c>: padding probably
        const char*         m_pDebugFileName;    ///< <c>0x38</c>: the debug filename eg: t2r/src/game/scriptx/ss/ss-tag-as-hero.dcx
        const char*         m_pErrorName;        ///< <c>0x40</c>: used to store any error if any
        u64                 m_padding;           ///< <c>0x48</c>: padding probably
 
        SsState* resolve_state(StringId name);
    };


    // ============================================================================
    // ПЕРЕМЕННЫЕ STATE SCRIPT
    // ============================================================================
    
    /// @brief Список переменных, объявленных в state script
    struct SsDeclarationList //0x10
    {
        u32             m_totalDeclarationSize; ///< <c>0x00</c>: total size of all declarations
        u32             m_numDeclarations;      ///< <c>0x04</c>: number of declared vars in the table
        SsDeclaration*  m_pDeclarations;        ///< <c>0x08</c>: ptr to the list of declarations
    };

    /// @brief Одна переменная state script
    /// @example (define health 100) → health: int32 = 100
    struct SsDeclaration //0x30
    {
        sid64       m_declId;               ///< <c>0x00</c>: StringId of the declaration name
        const char* m_declIdString;         ///< <c>0x08</c>: padding probably
        sid64       m_declTypeId;           ///< <c>0x10</c>: StringId of the declaration type eg: boolean, int32, float etc..
        u16         m_varSizeSum;           ///< <c>0x18</c>: size of the variable in bytes
        u16         m_isVar;                ///< <c>0x1A</c>: is variable ?
        u32         m_always0;              ///< <c>0x1C</c>: always 0 ?
        void*       m_pDeclValue;           ///< <c>0x20</c>: ptr to the declaration value
        u64         m_always0x80;           ///< <c>0x28</c>: always 0x80 ?
    };

    // ============================================================================
    // ОПЦИИ STATE SCRIPT
    // ============================================================================
    
    struct SsOptions //0x50
    {
        const char*  m_optionString;
        u64          m_unknownFlags;
        u64          m_always0_1;
        SymbolArray* m_pSymbolArray;
        SymbolArray* m_symbolArray2;        ///< overwhelmingly nullptr
        SymbolArray* m_symbolArray3;        ///< overwhelmingly nullptr
        SymbolArray* m_symbolArray4;        ///< overwhelmingly nullptr
        u32          m_always5;             ///< <c>0x38</c>: unk number
        u32          m_mostly0;             ///< sometimes 16, 32, 1, 13, 21, 8
        u64          m_mostly0Rarely1;
        u64          m_always0_2;
    };

    /// @brief Массив идентификаторов (символов)
    struct SymbolArray //0x10
    {
        u32         m_numEntries;           ///< <c>0x00</c>: number of entries
        u32         m_unk;                  ///< <c>0x04</c>: always 0 ?
        sid64*      m_pSymbols;             ///< <c>0x08</c>: ptr to the symbols
    };

    // ============================================================================
    // ТРЕКИ (анимации, звуки, эффекты)
    // ============================================================================
    
    /// @brief Группа параллельных треков    
    struct SsTrackGroup
    {
        u64                  m_always0;            ///< <c>0x00</c>:
        u16                  m_totalLambdaCount;   ///< <c>0x08</c>: unk number
        i16                  m_numTracks;          ///< <c>0x0A</c>: num tracks
        u32                  m_padding;            ///< <c>0x1C</c>: padding probably
        SsTrack*             m_aTracks;            ///< <c>0x10</c>: ptr to an array of Tracks
        const char*          m_name;               ///< <c>0x18</c>: eg: ss-fp-test initial (on (start))
        u64                  m_always0_1;          ///< <c>0x20</c>:
        u64                  m_always0_2;          ///< <c>0x28</c>:
        const ScriptLambda*  m_rareScriptLambda;   ///< <c>0x30</c>:
    };

    // ============================================================================
    // СОСТОЯНИЯ И ПЕРЕХОДЫ
    // ============================================================================

    enum class BlockType : i32 {
        Start, End, Event, Update, Virtual, 
        Code, // added by me 
        Exit, // added by me 
        Post // added by me
    };

    /// @brief Одно состояние конечного автомата
    /// @example state idle { on (start) { ... } on (damage) { ... } }    
    struct SsState //0x18
    {
        sid64       m_stateId;              ///< <c>0x00</c>: StringId64 of the state name
        i64         m_numSsOnBlocks;        ///< <c>0x08</c>: numSsOnBlocks
        SsOnBlock*  m_pSsOnBlocks;          ///< <c>0x10</c>: ptr to the SsOnBlocks table

        /// @deprecatred For back compatibility
        bool is_virtual() { return false; }
        /// @deprecatred For back compatibility
        bool is_override() { return false; }

        StringId state_id() { return  StringId(m_stateId);}
        std::string name() { return  StringId(m_stateId).to_string();}

        SsOnBlock* lookup(BlockType type);
        SsOnBlock* lookup(BlockType type, StringId event_id);

        ScriptLambda* get_trans_function();
        ScriptLambda* get_enter_function();
        ScriptLambda* get_exit_function();
        ScriptLambda* get_post_function();
        ScriptLambda* get_code_function();
        ScriptLambda* get_event_handler(StringId event_id);
    };

    inline const char* block_type_to_string(BlockType type) {
       switch (type) {
        case BlockType::Start: return "start";
        case BlockType::End: return "end";
        case BlockType::Event: return "event";
        case BlockType::Update: return "update";
        case BlockType::Virtual: return "virtual";
        default: 
            return "unknown";
       }
    }

    /// @brief Блок обработки события в состоянии
    /// @example (on (start) (lambda () (print "entered idle")))
    struct SsOnBlock //
    {
        BlockType     m_blockType;       ///< <c>0x00</c>: //on start || on update || on event etc
        u32           m_always0;         ///< <c>0x04</c>: unk number
        sid64         m_blockEventId;    ///< <c>0x08</c>: UNSURE. Can be null. if its null there's no script lambda ptr
        ScriptLambda* m_pScriptLambda;   ///< <c>0x10</c>: ptr to the script Lambda
        SsTrackGroup  m_trackGroup;

        ScriptLambda* lambda() { return  (ScriptLambda*)m_pScriptLambda;}
        std::string name() const { return m_blockType != BlockType::Event ? block_type_to_string(m_blockType) :  fmt::format("{} {}", block_type_to_string(m_blockType), StringId(m_blockEventId)); }
    };

    // ============================================================================
    // ТРЕКИ (анимации, звуки, эффекты)
    // ============================================================================

    /// @brief Один трек (дорожка анимации/звука)    
    struct SsTrack //0x18
    {
        sid64     m_trackId;             ///< <c>0x00</c>: StringId64 of the track name
        u16       m_trackIdx;
        i16       m_totalLambdaCount;
        u32       m_padding;
        SsLambda* m_pSsLambda;           ///< <c>0x10</c>: ptr to the SsLambda table
    };

    /// @brief Лямбда внутри трека
    struct SsLambda //0x10
    {
        ScriptLambda*  m_pScriptLambda;      ///< <c>0x00</c>: ptr to the script lambda of the track
        u64            m_someSortOfCounter;  /// no clue, seems to be counting up, but there are holes in the counting
    };

    // ============================================================================
    // ИСПОЛНЯЕМАЯ ФУНКЦИЯ (LAMBDA)
    // ============================================================================



    /// @brief Скомпилированная функция (лямбда)
    /// @example (lambda (x) (+ x 1))    
    struct ScriptLambda //0x50
    {
        u64*   m_pInstruction;    // instruction ptr
        u64*   m_pSymbols;        // symbol table ptr
        sid64  m_typeId;          // SID("function")
        u64    m_sum;             // 12 + (4 * number of instructions) + (4 * number of symbol table entries)
        sid64  m_funcName;        // can be 0
        u64    m_instructionFlag; // 0xDEADBEEF1337FOOD
        u32    m_always0_2;
        u32    m_numInstructions; // number of instructions
        i64    m_neg1;
        u64    m_sidGlobal;       // either SID("global") if in global scope or something else if inside state-script
        u64    m_always0_3;

        Instruction* get_code_ptr() const { return (Instruction*)m_pInstruction;}
        u64* get_symbols_ptr() const { return m_pSymbols;}
    };
    
    inline SsOnBlock* SsState::lookup(BlockType type) {
        for (int i=0;i<m_numSsOnBlocks;i++)
            if (m_pSsOnBlocks[i].m_blockType == type)
                return &m_pSsOnBlocks[i];
        return nullptr;
    }
    inline SsOnBlock* SsState::lookup(BlockType type, StringId event_id) {
        for (int i=0;i<m_numSsOnBlocks;i++)
            if (m_pSsOnBlocks[i].m_blockType == type && m_pSsOnBlocks[i].m_blockEventId == event_id)
                return &m_pSsOnBlocks[i];
        return nullptr;
    }

    inline ScriptLambda* SsState::get_trans_function() { auto e=lookup(BlockType::Update); return e!=nullptr ? e->m_pScriptLambda : nullptr;};
    inline ScriptLambda* SsState::get_enter_function() { auto e=lookup(BlockType::Start); return e!=nullptr ? e->m_pScriptLambda : nullptr;};
    inline ScriptLambda* SsState::get_exit_function() { auto e=lookup(BlockType::Exit); return e!=nullptr ? e->m_pScriptLambda : nullptr;};
    inline ScriptLambda* SsState::get_post_function() { auto e=lookup(BlockType::Post); return e!=nullptr ? e->m_pScriptLambda : nullptr;};
    inline ScriptLambda* SsState::get_code_function() { auto e=lookup(BlockType::Code); return e!=nullptr ? e->m_pScriptLambda : nullptr;};
    inline ScriptLambda* SsState::get_event_handler(StringId event_id) { auto e=lookup(BlockType::Event, event_id); return e!=nullptr ? e->m_pScriptLambda : nullptr;};


    inline SsState* StateScript::resolve_state(StringId state_id) {
        for (int i=0;i<m_stateCount;i++) {
            if (m_pSsStateTable[i].state_id() == state_id)
                return &m_pSsStateTable[i];
        }
        return nullptr;
    }
#endif // DCSCRIPT_H

}

