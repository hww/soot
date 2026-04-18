#include "sootc/Compiler/Compiler.hpp"
#include "common/sooti/Reader.hpp"
#include "sootc/Env/Env.hpp"
#include "sootc/Env/GlobalEnv.hpp"
#include <fstream>

namespace sootc {

Compiler::Compiler(TypeSystem& ts) : ts_(ts) {}

std::expected<std::unique_ptr<BinaryFile>, std::string> Compiler::compile_file(const std::filesystem::path& path, Env* env) {
    
    // 1. Читаем файл через твой Reader
    script::Reader reader;
    // Читаем формы (S-выражения) из файла
    auto forms = reader.read_from_file({ path.string() }, true, false);
    if (forms.is_null()) {
        return std::unexpected("Failed to read or parse file: " + path.string());
    }

    return compile_file(forms, env, path.string());
}

std::expected<std::unique_ptr<BinaryFile>, std::string> Compiler::compile_file(script::Object& forms, Env* env, const std::string filename)
{
    // 2. Создаем окружения
    FileEnv file_env(env, filename);

    // 3. Создаем FileCompiler (линкер)
    // Мы отдаем ему ссылку на этот Compiler, чтобы он мог вызывать compile() для IR
    FileCompiler file_compiler(ts_, this);

    // 4. Запускаем процесс компиляции файла
    // Твой FileCompiler внутри сделает: Declare -> Resolve -> Build -> MakeBinary
    return file_compiler.compile_file(forms, &file_env, filename);
}


IR_Value* Compiler::compile(const script::Object& form, Env* env) {
    // 1. Если это символ — значит, это обращение к переменной
    if (form.is_symbol()) {
        return new IR_SymbolReference(form, env);
    }

    // 2. Если это не список (не pair), значит это литерал (число, строка и т.д.)
    if (!form.is_pair()) {
        return new IR_LiteralValue(form);
    }

    // 3. Если это список, смотрим на первый элемент (keyword)
    auto head = form.as_pair()->car;
    auto rest = form.as_pair()->cdr;

    if (head.is_symbol()) {
        std::string keyword = head.as_symbol();

        // Определение переменной или функции: (define name value)
        if (keyword == "define") {
            return compile_define(form, rest, env);
        }

        // Создание функции: (lambda (args) body)
        if (keyword == "lambda") {
            return compile_lambda(form, rest, env);
        }

        // Конечный автомат: (state-script name (initial ...) ...)
        if (keyword == "state-script") {
            return compile_state_script(form, rest, env);
        }
        
        // Вызов функции: (func-name arg1 arg2)
        return compile_call(form, rest, env);
    }

    return nullptr; 
}
IR_Value* Compiler::compile_define(const script::Object& form, const script::Object& rest, Env* env) { return  nullptr; }
IR_Value* Compiler::compile_lambda(const script::Object& form, const script::Object& rest, Env* env) { return  nullptr; }
IR_Value* Compiler::compile_state_script(const script::Object& form, const script::Object& rest, Env* env) { return  nullptr; }
IR_Value* Compiler::compile_call(const script::Object& form, const script::Object& rest, Env* env) { return  nullptr; }
} // namespace sootc