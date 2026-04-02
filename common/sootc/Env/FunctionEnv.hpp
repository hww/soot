#pragma once

#include "common/sootc/Env/DeclareEnv.hpp"
#include "common/sootc/Env/Label.hpp"
#include "common/type_system/TypeSpec.hpp" 
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace sootc {

class IR_Node;
class IR_Reg;
class IR_GotoLabel;
class IR_ConditionalBranch;
class StackVarAddrVal;
struct Label;

struct UnresolvedGoto {
    IR_GotoLabel* ir = nullptr;
    std::string label;
};

struct UnresolvedConditionalGoto {
    IR_ConditionalBranch* ir = nullptr;
    std::string label;
};

struct IRegConstraint {
    // Упрощенная версия, позже можно расширить
    std::string description;
};

struct AllocationResult {
    // Результат аллокации регистров
    bool success = true;
};

/*!
 * An Env for a function/method/state
 */
class FunctionEnv : public DeclareEnv {
public:
    FunctionEnv(Env* parent, const std::string& name);
    ~FunctionEnv() = default;

    // ========================================================================
    // Env interface
    // ========================================================================
    std::string print() const override;
    IR_Value* lookup(const std::string& name) override;
    void bind(const std::string& name, IR_Value* val) override;
    std::string get_value_name(IR_Value* value) const override;

    // ========================================================================
    // Label management
    // ========================================================================
    std::unordered_map<std::string, Label>& get_label_map() override { return m_labels; }
    Label* alloc_unnamed_label();
    Label* get_label(const std::string& name);

    // ========================================================================
    // Code emission
    // ========================================================================
    void emit(const script::Object& form, std::unique_ptr<IR_Node> ir) override;
    void finish();
    const std::vector<std::unique_ptr<IR_Node>>& code() const { return m_code; }
    const std::vector<script::Object>& code_source() const { return m_code_debug_source; }

    // ========================================================================
    // Register management
    // ========================================================================
    IR_Reg* make_ireg(const TypeSpec& ts, RegClass reg_class) override;
    IR_Reg* alloc_reg(Type* type);
    const std::vector<std::unique_ptr<IR_Reg>>& reg_vals() const { return m_iregs; }
    int max_vars() const { return m_iregs.size(); }

    // ========================================================================
    // Constraints and allocation
    // ========================================================================
    const std::vector<IRegConstraint>& constraints() const { return m_constraints; }
    void constrain(const IRegConstraint& c) { m_constraints.push_back(c); }
    void set_allocations(AllocationResult&& result) { m_regalloc_result = std::move(result); }
    const AllocationResult& alloc_result() const { return m_regalloc_result; }

    // ========================================================================
    // Parameters
    // ========================================================================
    void define_argument(const std::string& name, Type* type, int reg_index);
    const std::vector<IR_Reg*>& params() const { return m_params; }
    IR_Reg* lookup_param(const std::string& name) const;

    // ========================================================================
    // Stack management
    // ========================================================================
    struct StackSpace {
        int start_slot;
        int slot_count;
    };
    
    bool needs_aligned_stack() const { return m_aligned_stack_required; }
    void require_aligned_stack() { m_aligned_stack_required = true; }
    
    StackSpace allocate_aligned_stack_space(int size_bytes, int align_bytes);
    StackVarAddrVal* allocate_aligned_stack_variable(const TypeSpec& ts, int size_bytes, int align_bytes);
    StackVarAddrVal* allocate_stack_singleton(const TypeSpec& ts, int size_bytes, int align_bytes);
    int stack_slots_used_for_stack_vars() const { return m_stack_var_slots_used; }

    // ========================================================================
    // Gotos resolution
    // ========================================================================
    std::vector<UnresolvedGoto> unresolved_gotos;
    std::vector<UnresolvedConditionalGoto> unresolved_cond_gotos;

    // ========================================================================
    // Getters
    // ========================================================================
    const std::string& name() const { return m_name; }
    const std::vector<IR_Value*>& symbols() const override { return m_ordered_symbols; }
    const std::unordered_map<std::string, IR_Value*>& symbols_map() const { return m_symbols_map; }

    // ========================================================================
    // Source form (для отладки)
    // ========================================================================
    void set_source_form(const script::Object& form) { m_source_form = form; }
    script::Object source_form() const { return m_source_form; }

    // ========================================================================
    // Return type
    // ========================================================================
    void set_return_type(Type* type) { m_return_type = type; }
    Type* get_return_type() const { return m_return_type; }

    // ========================================================================
    // Segment (для GOAL совместимости)
    // ========================================================================
    int segment = -1;
    void set_segment(int seg) { segment = seg; }
    int segment_for_static_data();

    // ========================================================================
    // Method info (для MethodEnv)
    // ========================================================================
    std::string method_of_type_name = "#f";
    TypeSpec method_function_type;
    std::optional<int> method_id;
    bool is_asm_func = false;
    bool asm_func_saved_regs = false;
    TypeSpec asm_func_return_type;

    // ========================================================================
    // Index in file
    // ========================================================================
    int idx_in_file = -1;

    // ========================================================================
    // Memory management
    // ========================================================================
    template <typename T, class... Args>
    T* alloc_val(Args&&... args) {
        std::unique_ptr<T> new_obj = std::make_unique<T>(std::forward<Args>(args)...);
        m_vals.push_back(std::move(new_obj));
        return static_cast<T*>(m_vals.back().get());
    }

    template <typename T, class... Args>
    T* alloc_env(Args&&... args) {
        std::unique_ptr<T> new_obj = std::make_unique<T>(std::forward<Args>(args)...);
        m_envs.push_back(std::move(new_obj));
        return static_cast<T*>(m_envs.back().get());
    }

    IR_Reg* push_reg_val(std::unique_ptr<IR_Reg> in);

    const std::vector<std::unique_ptr<IR_Node>>& nodes() const { return m_nodes; }
    std::vector<std::unique_ptr<IR_Node>>& nodes() { return m_nodes; }

    int get_max_param_index() const {
        int max_idx = -1;
        for (auto* reg : m_params) {
            if ((s32)reg->get_index() > max_idx) {
                max_idx = reg->get_index();
            }
        }
        return max_idx;
    }
protected:
    void resolve_gotos();

    std::string m_name;
    std::vector<std::unique_ptr<IR_Node>> m_nodes;  // промижуточный код
    std::vector<std::unique_ptr<IR_Node>> m_code;   // финальный код после оптимизаций
    std::vector<script::Object> m_code_debug_source;

    std::vector<std::unique_ptr<IR_Reg>> m_iregs;
    std::vector<std::unique_ptr<IR_Value>> m_vals;
    std::vector<std::unique_ptr<Env>> m_envs;
    std::vector<IRegConstraint> m_constraints;

    AllocationResult m_regalloc_result;

    bool m_aligned_stack_required = false;
    int m_stack_var_slots_used = 0;
    std::unordered_map<std::string, Label> m_labels;
    std::vector<std::unique_ptr<Label>> m_unnamed_labels;
    std::unordered_map<std::string, StackSpace> m_stack_singleton_slots;

    // Symbol table
    std::unordered_map<std::string, IR_Value*> m_symbols_map;
    std::vector<IR_Value*> m_ordered_symbols;

    // Parameters
    std::vector<IR_Reg*> m_params;
    std::unordered_map<std::string, IR_Reg*> m_param_map;

    // Other
    int m_next_reg = 0;
    script::Object m_source_form;
    Type* m_return_type = nullptr;

    // Константы
    static constexpr int TOP_LEVEL_SEGMENT = 2;
};

} // namespace sootc