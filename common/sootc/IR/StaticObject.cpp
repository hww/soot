// common/sootc/IR/StaticObject.cpp
#include "common/sootc/IR/StaticObject.hpp"
#include "common/sootc/Env/TypeEnv.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/MethodEnv.hpp"
#include "common/sootc/Env/StateEnv.hpp"
#include "fmt/format.h"

namespace sootc {

// ============================================================================
// StaticFloat
// ============================================================================

StaticFloat::StaticFloat(float val) : value(val) {}

std::string StaticFloat::print() const {
    return fmt::format("(sf {})", value);
}

StaticObject::LoadInfo StaticFloat::get_load_info() const {
    LoadInfo info;
    info.requires_load = true;
    info.load_size = 4;
    info.load_signed = false;
    info.prefer_xmm = true;
    return info;
}

void StaticFloat::generate(std::vector<u8>& data, std::vector<u64>& relocations) {
    (void)relocations;
    // Записываем 4 байта float
    const u8* ptr = reinterpret_cast<const u8*>(&value);
    for (size_t i = 0; i < sizeof(float); i++) {
        data.push_back(ptr[i]);
    }
}

u64 StaticFloat::get_addr_offset() const {
    return 0;
}

// ============================================================================
// StaticString
// ============================================================================

StaticString::StaticString(const std::string& str) : text(str) {}

std::string StaticString::print() const {
    return fmt::format("static-string \"{}\"", text);
}

StaticObject::LoadInfo StaticString::get_load_info() const {
    LoadInfo info;
    info.requires_load = false;
    return info;
}

void StaticString::generate(std::vector<u8>& data, std::vector<u64>& relocations) {
    (void)relocations;
    
    // 1. Type tag placeholder (8 байт)
    for (int i = 0; i < 8; i++) {
        data.push_back(0xBE);
    }
    
    // 2. Allocated size (4 байта)
    u32 size = static_cast<u32>(text.size());
    const u8* size_ptr = reinterpret_cast<const u8*>(&size);
    for (size_t i = 0; i < sizeof(u32); i++) {
        data.push_back(size_ptr[i]);
    }
    
    // 3. String data + null terminator
    for (char c : text) {
        data.push_back(static_cast<u8>(c));
    }
    data.push_back(0);
}

u64 StaticString::get_addr_offset() const {
    return 0;
}

// ============================================================================
// StaticStructure
// ============================================================================

StaticStructure::StaticStructure() = default;

std::string StaticStructure::print() const {
    return "static-structure";
}

StaticObject::LoadInfo StaticStructure::get_load_info() const {
    LoadInfo info;
    info.requires_load = false;
    return info;
}

void StaticStructure::generate(std::vector<u8>& out_data, std::vector<u64>& relocations) {
    // Копируем данные
    out_data.insert(out_data.end(), data.begin(), data.end());
    
    // Добавляем релокации для символов
    for (const auto& sym : symbols) {
        relocations.push_back(sym.offset);
    }
    
    // Добавляем релокации для указателей
    for (const auto& ptr : pointers) {
        relocations.push_back(ptr.offset_in_this);
    }
    
    // Добавляем релокации для функций
    for (const auto& func : functions) {
        relocations.push_back(func.offset_in_this);
    }
    
    // Добавляем релокации для методов
    for (const auto& method : methods) {
        relocations.push_back(method.offset_in_this);
    }
    
    // Добавляем релокации для состояний
    for (const auto& state : states) {
        relocations.push_back(state.offset_in_this);
    }
    
    // Добавляем релокации для типов
    for (const auto& type : types) {
        relocations.push_back(type.offset_in_this);
    }
}

u64 StaticStructure::get_addr_offset() const {
    return m_offset;
}

void StaticStructure::add_symbol(const std::string& name, u64 offset) {
    symbols.push_back({offset, name});
}

void StaticStructure::add_pointer(u64 offset_in_this, StaticStructure* dest, u64 offset_in_dest) {
    pointers.push_back({offset_in_this, dest, offset_in_dest});
}

void StaticStructure::add_function(FunctionEnv* func, u64 offset) {
    functions.push_back({offset, func});
}

void StaticStructure::add_method(MethodEnv* method, u64 offset) {
    methods.push_back({offset, method});
}

void StaticStructure::add_state(StateEnv* state, u64 offset) {
    states.push_back({offset, state});
}

void StaticStructure::add_type(TypeEnv* type, u64 offset) {
    types.push_back({offset, type});
}

// ============================================================================
// StaticBasic
// ============================================================================

StaticBasic::StaticBasic(const std::string& type_name) 
    : StaticStructure(), type_name(type_name) {
    // Временно: type tag будет добавлен позже через add_type
    // Для этого нужно найти TypeEnv по имени
}

u64 StaticBasic::get_addr_offset() const {
    return 0;  // BASIC_OFFSET в GOAL = 4 байта от начала объекта
}

// ============================================================================
// StaticPair
// ============================================================================

StaticPair::StaticPair() : StaticStructure() {
    // 2 указателя по 8 байт = 16 байт
    data.resize(16, 0);
}

u64 StaticPair::get_addr_offset() const {
    return 0;  // PAIR_OFFSET в GOAL = 8 байт от начала объекта
}

void StaticPair::set_car(StaticObject* car, u64 offset) {
    m_car = car;
    m_car_offset = offset;
    
    // Если car — структура, добавляем указатель
    if (auto* car_struct = dynamic_cast<StaticStructure*>(car)) {
        add_pointer(0, car_struct, offset);
    }
    // Иначе car — примитив, данные уже в data
}

void StaticPair::set_cdr(StaticObject* cdr, u64 offset) {
    m_cdr = cdr;
    m_cdr_offset = offset;
    
    // Если cdr — структура, добавляем указатель
    if (auto* cdr_struct = dynamic_cast<StaticStructure*>(cdr)) {
        add_pointer(8, cdr_struct, offset);
    }
}

} // namespace sootc