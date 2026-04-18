#pragma once

#include "common/sootc/Compiler/FileCompiler.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "sootc/Env/Env.hpp"
#include "sootc/Env/FileEnv.hpp"
#include "sootc/Env/GlobalEnv.hpp"
#include <string>
#include <filesystem>

namespace sootc {

class Compiler {
public:
    explicit Compiler(TypeSystem& ts);

    /**
     * @brief Главный метод: путь к файлу -> готовый бинарник DC00
     */
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(const std::filesystem::path& path, Env* env);
    
    /**
     * @brief Главный метод: путь к файлу -> готовый бинарник DC00
     */
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(script::Object& forms, Env* env, const std::string filename);


    // Этот метод нужен FileCompiler-у, когда тот обходит IR_Value
    IR_Value* compile(const script::Object& form, Env* env);

    // Доступ к систеиме типов
    TypeSystem& ts() { return  ts_; }
private:

    IR_Value* compile_define(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_lambda(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_state_script(const script::Object& form, const script::Object& rest, Env* env);
    IR_Value* compile_call(const script::Object& form, const script::Object& rest, Env* env);

    TypeSystem& ts_;
};

} // namespace sootc