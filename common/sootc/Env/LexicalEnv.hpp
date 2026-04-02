#pragma once

#include "common/sootc/Env/DeclareEnv.hpp"
#include "sootc/IR/IR_Value.hpp"

namespace sootc {

/*!
 * An Env for lexical scope.
 */
/*!
    // Пример вложенности
    (defun complex ()
      (let ((x 10))           // ← LexicalEnv для x
        (tagbody
          loop                // ← LabelEnv для метки loop
            (print x)         // ← использует x из LexicalEnv
            (go loop))))      // ← использует метку из LabelEnv
    
    // Цепочка Env:
    GlobalEnv
      └─ FileEnv
          └─ FunctionEnv
              └─ LexicalEnv (для let)   // хранит переменную x
                  └─ LabelEnv (для tagbody)  // хранит метку loop
*/
class LexicalEnv : public DeclareEnv {
public:
    explicit LexicalEnv(Env* parent) : DeclareEnv(EnvKind::LEXICAL_ENV, parent) {}
    
    // Лексические переменные (символ → регистр)
    std::unordered_map<script::InternedSymbolPtr, IR_Value*, 
                       script::InternedSymbolPtr::hash> vars;
    
    IR_Value* lexical_lookup(const script::Object& sym) override {  // ← IR_Value*, не IR_Reg*
        if (!sym.is_symbol()) return nullptr;
        auto it = vars.find(sym.as_symbol());
        if (it != vars.end()) return it->second;
        return parent() ? parent()->lexical_lookup(sym) : nullptr;
    }
        
    std::string print() const override { return "lexical-env"; }
};

} // namespace sootc