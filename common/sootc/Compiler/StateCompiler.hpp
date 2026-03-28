#pragma once

#include "common/carbon/files/StateDesc.hpp"
#include "common/carbon/files/Definition.hpp"
#include <vector>

namespace carbon::files {

class StateBuilder {
public:
    StateBuilder(const std::string& name, const std::string& parent);
    
    void set_flags(StateFlags flags);
    
    // Добавление обработчиков (по индексам CODE_ID, ENTER_ID, etc.)
    void set_handler(int id, std::vector<Instruction> code);
    void set_handler(int id, FunctionDesc* function);
    
    // Удобные методы
    void set_code(std::vector<Instruction> code) { set_handler(StateDesc::CODE_ID, code); }
    void set_enter(std::vector<Instruction> code) { set_handler(StateDesc::ENTER_ID, code); }
    void set_exit(std::vector<Instruction> code) { set_handler(StateDesc::EXIT_ID, code); }
    void set_trans(std::vector<Instruction> code) { set_handler(StateDesc::TRANS_ID, code); }
    void set_post(std::vector<Instruction> code) { set_handler(StateDesc::POST_ID, code); }
    void set_event(std::vector<Instruction> code) { set_handler(StateDesc::EVENT_ID, code); }
    
    // Построение
    std::vector<u8> build();
    
    StateDesc* get_state_desc() { return state_desc_.get(); }
    
private:
    std::unique_ptr<StateDesc> state_desc_;
    std::vector<Definition> handlers_;  // до 6 обработчиков
    std::vector<std::vector<u8>> handler_data_;
};

} // namespace carbon::files