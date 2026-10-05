#include "LetNode.hpp"
#include "FunctionNode.hpp"
#include "VariableNode.hpp"
#include "common/util/Log.hpp"
#include "vm/Instructions.hpp"

namespace sootc {

    void LetNode::emit(FunctionNode &fn) {
        if (!m_body) {
            lg::warn("LetNode::emit: no body");
            return;
        }

        // 1. Объявляем все переменные в FunctionNode.
        //    Каждая получает локальный регистр (r0, r1, ...).
        //    ВАЖНО: это должно быть ДО emit() значений, чтобы
        //    alloc_temp_reg() выдавал регистры ПОСЛЕ локалов.
        for (auto &b : m_bindings) { fn.add_local_variable(b.name, /* type */ nullptr); }

        // 2. Для каждой переменной: вычисляем значение и Move в её регистр.
        for (auto &b : m_bindings) {
            b.value->emit(fn);
            u8 value_reg = fn.get_temp_reg(b.value.get());
            u8 local_reg = fn.get_variable_reg(b.name);

            if (value_reg != local_reg) {
                fn.add_instruction(Opcode::Move, local_reg, value_reg, 0);
            }
        }

        // 3. Тело let. Результат — результат тела.
        m_body->emit(fn);
        u8 body_reg = fn.get_temp_reg(m_body.get());
        fn.set_temp_reg(this, body_reg);

        // 4. Тип — как у тела.
        m_type = m_body->get_type();
    }

    std::string LetNode::to_string() const {
        std::string result = "(let (";
        for (size_t i = 0; i < m_bindings.size(); ++i) {
            if (i != 0) result += " ";
            result += "(" + m_bindings[i].name + " " + m_bindings[i].value->to_string() + ")";
        }
        result += ") ";
        result += m_body ? m_body->to_string() : "none";
        result += ")";
        return result;
    }

} // namespace sootc