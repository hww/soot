#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/Label.hpp"
#include "common/sootc/Env/FileEnv.hpp"
#include "common/sootc/IR/IR_Node.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include "common/sootc/IR/StackVarAddrVal.hpp"  
#include "common/type_system/TypeSpec.hpp"
#include "fmt/format.h"
#include "type_system/TypeSystem.hpp"

namespace sootc {

// ============================================================================
// Construction
// ============================================================================

FunctionEnv::FunctionEnv(Env* parent, const std::string& name)
    : DeclareEnv(EnvKind::FUNCTION_ENV, parent), m_name(name) {}

// ============================================================================
// Env interface
// ============================================================================

std::string FunctionEnv::print() const {
    return fmt::format("FunctionEnv(name={}, params={}, code={})", 
                       m_name, m_params.size(), m_code.size());
}

IR_Value* FunctionEnv::lookup(const std::string& name) {
    // 1. Сначала параметры
    auto param_it = m_param_map.find(name);
    if (param_it != m_param_map.end()) {
        return param_it->second;
    }
    
    // 2. Локальные символы
    auto it = m_symbols_map.find(name);
    if (it != m_symbols_map.end()) {
        return it->second;
    }
    
    // 3. Родитель
    return m_parent ? m_parent->lookup(name) : nullptr;
}

void FunctionEnv::bind(const std::string& name, IR_Value* val) {
    if (m_symbols_map.find(name) == m_symbols_map.end()) {
        m_ordered_symbols.push_back(val);
    }
    m_symbols_map[name] = val;
}

std::string FunctionEnv::get_value_name(IR_Value* value) const {
    for (const auto& [name, val] : m_symbols_map) {
        if (val == value) {
            return name;
        }
    }
    for (const auto* param : m_params) {
        if (param == value) {
            return "param";
        }
    }
    return "<unknown>";
}

// ============================================================================
// Label management
// ============================================================================

Label* FunctionEnv::alloc_unnamed_label() {
    m_unnamed_labels.emplace_back(std::make_unique<Label>());
    return m_unnamed_labels.back().get();
}

Label* FunctionEnv::get_label(const std::string& name) {
    return &m_labels[name];
}

// ============================================================================
// Code emission
// ============================================================================

void FunctionEnv::emit(const script::Object& form, std::unique_ptr<IR_Node> ir) {
    // Добавляем constraints если нужно
    // ir->add_constraints(&m_constraints, m_code.size());
    m_code.push_back(std::move(ir));
    m_code_debug_source.push_back(form);
}

void FunctionEnv::finish() {
    resolve_gotos();
}

void FunctionEnv::resolve_gotos() {
    for (auto& gt : unresolved_gotos) {
        auto it = m_labels.find(gt.label);
        if (it == m_labels.end()) {
            throw std::runtime_error("Invalid goto: " + gt.label);
        }
        // gt.ir->resolve(&it->second);
    }

    for (auto& gt : unresolved_cond_gotos) {
        auto it = m_labels.find(gt.label);
        if (it == m_labels.end()) {
            throw std::runtime_error("Invalid when-goto destination: " + gt.label);
        }
        // gt.ir->label = it->second;
        // gt.ir->mark_as_resolved();
    }
}

// ============================================================================
// Register management
// ============================================================================

IR_Reg* FunctionEnv::make_ireg(const TypeSpec& ts, RegClass reg_class) {
    (void)ts;
    (void)reg_class;
    // Временно: просто выделяем регистр
    return alloc_reg(nullptr);
}

IR_Reg* FunctionEnv::alloc_reg(Type* type) {
    auto reg = std::make_unique<IR_Reg>(type, m_next_reg++);
    IR_Reg* result = reg.get();
    m_iregs.push_back(std::move(reg));
    return result;
}

IR_Reg* FunctionEnv::push_reg_val(std::unique_ptr<IR_Reg> in) {
    m_iregs.push_back(std::move(in));
    return m_iregs.back().get();
}

// ============================================================================
// Parameters
// ============================================================================

void FunctionEnv::define_argument(const std::string& name, Type* type, int reg_index) {
    auto* reg = new IR_Reg(type, reg_index, true);
    m_params.push_back(reg);
    m_param_map[name] = reg;
    bind(name, reg);
}

IR_Reg* FunctionEnv::lookup_param(const std::string& name) const {
    auto it = m_param_map.find(name);
    return it != m_param_map.end() ? it->second : nullptr;
}

// ============================================================================
// Stack management
// ============================================================================

FunctionEnv::StackSpace FunctionEnv::allocate_aligned_stack_space(int size_bytes, int align_bytes) {
    require_aligned_stack();
    
    int align_slots = (align_bytes + 8 - 1) / 8;  // 8 bytes per slot
    while (m_stack_var_slots_used % align_slots) {
        m_stack_var_slots_used++;
    }
    
    while (size_bytes % align_bytes) {
        size_bytes++;
    }
    
    int slots_used = (size_bytes + 8 - 1) / 8;
    StackSpace result;
    result.slot_count = slots_used;
    result.start_slot = m_stack_var_slots_used;
    m_stack_var_slots_used += slots_used;
    return result;
}

StackVarAddrVal* FunctionEnv::allocate_aligned_stack_variable(const TypeSpec& ts,
                                                              int size_bytes,
                                                              int align_bytes) {
    auto space = allocate_aligned_stack_space(size_bytes, align_bytes);
    // Нужно получить Type* из TypeSpec
    Type* type = TypeSystem::instance().lookup_type(ts); 
    return alloc_val<StackVarAddrVal>(type, space.start_slot, space.slot_count);
}

StackVarAddrVal* FunctionEnv::allocate_stack_singleton(const TypeSpec& ts,
                                                       int size_bytes,
                                                       int align_bytes) {
    const auto& existing = m_stack_singleton_slots.find(ts.print());
    if (existing == m_stack_singleton_slots.end()) {
        auto space = allocate_aligned_stack_space(size_bytes, align_bytes);
        m_stack_singleton_slots[ts.print()] = space;
        Type* type = TypeSystem::instance().lookup_type(ts); 
        return alloc_val<StackVarAddrVal>(type, space.start_slot, space.slot_count);
    } else {
        Type* type = TypeSystem::instance().lookup_type(ts); 
        return alloc_val<StackVarAddrVal>(type, existing->second.start_slot, existing->second.slot_count);
    }
}

// ============================================================================
// Segment
// ============================================================================

int FunctionEnv::segment_for_static_data() {
    if (segment == TOP_LEVEL_SEGMENT) {
        auto* fe = file_env();
        return fe ? fe->default_segment() : 0;
    }
    return segment;
}

} // namespace sootc