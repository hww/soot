#pragma once

#include "common/sootc/Env/Env.hpp"
#include "common/sootc/IR/IR_Value.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "common/sootc/Env/FunctionEnv.hpp"
#include "common/sootc/Env/FileEnv.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sootc {

class FileEnv;

class GlobalEnv : public Env {
public:
    GlobalEnv();
    ~GlobalEnv() {
        // Тело может быть пустым, но наличие деструктора важно
        // Он требует полного определения FileEnv и FunctionEnv        
    }

    std::string print() const override;
    IR_Reg* make_ireg(const TypeSpec& ts, RegClass reg_class) override;
    void constrain_reg(const IRegConstraint& constraint) override;
    IR_Reg* lexical_lookup(const script::Object& sym) override;
    BlockEnv* find_block(const std::string& name) override;

    FileEnv* add_file(std::string name);
    std::vector<std::string> list_files_with_prefix(const std::string& prefix);
    std::vector<std::unique_ptr<FileEnv>>& get_files();

private:
    std::vector<std::unique_ptr<FileEnv>> m_files;
};

} // namespace sootc