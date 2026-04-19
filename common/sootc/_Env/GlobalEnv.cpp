#include "common/sootc/Env/GlobalEnv.hpp"
#include "common/sootc/Env/FileEnv.hpp"
#include "common/sootc/Env/Env.hpp"
#include "common/sootc/IR/IR_Value.hpp"

namespace sootc {

GlobalEnv::GlobalEnv() 
    : Env(EnvKind::GLOBAL_ENV, nullptr) {
    // Инициализация
}

std::string GlobalEnv::print() const {
    return fmt::format("GlobalEnv(files={})", m_files.size());
}

IR_Reg* GlobalEnv::make_ireg(const TypeSpec& ts, RegClass reg_class) {
    (void)ts;
    (void)reg_class;
    throw std::runtime_error("Cannot allocate register in GlobalEnv");
}

void GlobalEnv::constrain_reg(const IRegConstraint& constraint) {
    (void)constraint;
    throw std::runtime_error("Cannot constrain register in GlobalEnv");
}

IR_Reg* GlobalEnv::lexical_lookup(const script::Object& sym) {
    (void)sym;
    return nullptr;
}

BlockEnv* GlobalEnv::find_block(const std::string& name) {
    (void)name;
    return nullptr;
}

FileEnv* GlobalEnv::add_file(std::string name) {
    auto file = std::make_unique<FileEnv>(this, std::move(name));
    FileEnv* result = file.get();
    m_files.push_back(std::move(file));
    return result;
}

std::vector<std::string> GlobalEnv::list_files_with_prefix(const std::string& prefix) {
    std::vector<std::string> matches;
    for (const auto& file : m_files) {
        if (file->name().rfind(prefix, 0) == 0) {
            matches.push_back(file->name());
        }
    }
    return matches;
}

std::vector<std::unique_ptr<FileEnv>>& GlobalEnv::get_files() {
    return m_files;
}

} // namespace sootc