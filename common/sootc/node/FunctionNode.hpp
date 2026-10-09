#pragma once

#include "Node.hpp"
#include "carbon/vm/Instructions.hpp"
#include "file/ProgramBinaryElement.hpp"
#include "sootc/libs/CompareOp.hpp"
#include "sootc/libs/Parameter.hpp"
#include "sootc/libs/VariableInfo.hpp"
#include "sootc/node/Node.hpp"
#include "type_system/Type.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

using namespace carbon;


namespace sootc {

    struct StringsTable;
    class ExpressionNode;

    /*
     * ALLOW_SIMPLE_ARGUMENT_SYNTAX
     * ----------------------------
     * a - параметр без типа
     * Это макрос(#if), не определён — значит,
     * по                                  умолчанию false
     *     .Компилятор требует типы у всех параметров :
     *
     * ((x int)(y int)) — работает
     *     .
     *
     * (x y) — ошибка Invalid argument definition x.
     *
     * Если определить ALLOW_SIMPLE_ARGUMENT_SYNTAX как 1 — параметры без типа будут
     *                     приниматься,
     * и тип будет взят из builder.parse_type(arg, ...).Но parse_type от символа
     *     x — что вернёт
     * ? Скорее всего — nullptr или object
     * .Потом add_parameter(name, type) с nullptr — упадёт или даст мусор.
     */

    // #define ALLOW_SIMPLE_ARGUMENT_SYNTAX 1


    class FunctionNode : public Node {
        std::string m_name;
        bool        m_exported = false;

        /// @brief Type this function is a method of, or "" for a plain function.
        /// @details Set by NodeBuilder::build_defmethod. The full name of a method
        ///          is "<type>-<method>"; the type itself is stored here so that
        ///          the method can be looked up in TypeSystem without string surgery.
        ///
        ///          For a plain function this is empty.
        std::string m_method_of_type;

        // Параметры
        std::vector<Parameter *>            m_params;
        std::unordered_map<std::string, u8> m_param_map;

        // Возвращаемый результат
        Type *m_return_type = nullptr;

        // Переменные (локальные + параметры)
        std::vector<VariableInfo>               m_variables;
        std::unordered_map<std::string, size_t> m_variable_index;
        size_t                                  m_param_count = 0;

        // Временные регистры (результаты выражений)
        int                                  m_next_temp_reg = 0;
        std::unordered_map<const Node *, u8> m_temp_regs;

        // Генерация кода
        std::vector<Instruction> m_instructions;
        std::vector<u64>         m_constants;
        std::vector<u8>          m_constants_kind;

        // Branch resolution
        std::unordered_map<std::string, u32>     m_labels;
        std::vector<std::pair<std::string, u32>> m_unresolved_branches;
        int                                      m_label_counter = 0;

        // Тело функции
        std::unique_ptr<ExpressionNode> m_body;
        StringsTable                    *m_state = nullptr;
    protected:
        void update_self_cache() override { m_cached_function = this; }

    public:
        explicit FunctionNode(const std::string &name);
        ~FunctionNode() = default;

        const char *node_type() const override { return "FunctionNode"; }
        std::string to_string() const override;

        // ========================================================================
        // Имя
        // ========================================================================
        const std::string &name() const { return m_name; }
        void               set_name(std::string name) { m_name = std::move(name); }
        void               set_exported(bool e) { m_exported = e; }
        bool               is_exported() const { return m_exported; }

        // ========================================================================
        // Method-of-type
        // ========================================================================
        /// @brief Empty for plain functions, set for methods.
        /// @details See the field comment above.
        const std::string &method_of_type() const { return m_method_of_type; }
        void               set_method_of_type(std::string t) { m_method_of_type = std::move(t); }
        bool               is_method() const { return !m_method_of_type.empty(); }

        // ========================================================================
        // Параметры
        // ========================================================================
        void   add_parameter(const std::string &name, Type *type);
        size_t param_count() const { return m_param_count; }
        // Параметры хранятся в m_variables — первые m_param_count элементов.
        std::vector<const VariableInfo *> parameters() const {
            std::vector<const VariableInfo *> result;
            result.reserve(m_param_count);
            for (size_t i = 0; i < m_param_count; ++i) { result.push_back(&m_variables[i]); }
            return result;
        }

        // ========================================================================
        // Локальные переменные
        // ========================================================================
        void                add_local_variable(const std::string &name, Type *type);
        const VariableInfo *lookup_variable(const std::string &name) const;
        u8                  get_variable_reg(const std::string &name) const;
        Type               *get_variable_type(const std::string &name) const;

        // ========================================================================
        // Временные регистры (для результатов выражений)
        // ========================================================================
        u8   alloc_temp_reg(Type *type);
        void set_temp_reg(const Node *node, u8 reg);
        u8   get_temp_reg(const Node *node) const;
        /// @brief Return true if the node has a temp register assigned.
        bool has_temp_reg(const Node *node) const {
            return m_temp_regs.find(node) != m_temp_regs.end();
        }

        // ========================================================================
        // Общий доступ к регистрам
        // ========================================================================
        u8   get_reg(const Node *node) const;
        void set_reg(const Node *node, u8 reg) { set_temp_reg(node, reg); }

        // ========================================================================
        // Возвращаемый тип
        // ========================================================================
        void  set_return_type(Type *type) { m_return_type = type; }
        Type *get_return_type() const { return m_return_type; }

        void         set_global_state(StringsTable *s) { m_state = s; }
        StringsTable *global_state() const { return m_state; }

        // ========================================================================
        // Константы
        // ========================================================================
        enum class ConstKind { INT, FLOAT, STRING, SID };
        u16 add_constant(u64 value, ConstKind kind = ConstKind::INT);

        // ========================================================================
        // Инструкции
        // ========================================================================
        void add_instruction(Opcode op, u8 dest, u8 src1 = 0, u8 src2 = 0);
        void add_instruction_imm_u16(Opcode op, u8 dest, u16 imm);

        // ========================================================================
        // Метки
        // ========================================================================
        void        add_label(const std::string &name);
        void        add_branch_reference(const std::string &label);
        void        resolve_branches();
        void        add_compare(u8 left, u8 right, CompareOp op);
        std::string create_unique_label(const std::string &prefix);

        // ========================================================================
        // Генерация кода
        // ========================================================================
        void set_body(std::unique_ptr<ExpressionNode> body);
        void emit_body(ExpressionNode *body);
        void emit_body();

        // ========================================================================
        // Сериализация
        // ========================================================================
        ProgramBinaryElement generate(StringsTable &state) override {
            return build_binary(m_name, state);
        }

    private:
        ProgramBinaryElement build_binary(const std::string &module_name, StringsTable &state);

        // ========================================================================
        // Доступ к результатам
        // ========================================================================
    public:
        const std::vector<Instruction> &instructions() const { return m_instructions; }
        const std::vector<u64>         &constants() const { return m_constants; }
    };

} // namespace sootc