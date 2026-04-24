// sootc/compiler/Compiler.hpp
#pragma once

#include "Log.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/soot/Object.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include "repl/config.h"
#include "repl/repl_wrapper.h"
#include "sootc/compiler/MakeSystem.hpp"
#include "sootc/libs/CompilerException.hpp"
#include "sootc/node/GlobalNode.hpp"
#include "sootc/node/NoneNode.hpp"
#include <filesystem>
#include <memory>
#include <expected>
#include <optional>
#include <vector>

namespace sootc {


// Forward declaration
enum class CompilerMode;

// Статус выполнения REPL
enum class ReplStatus {
    OK,
    ERROR,
    WANT_EXIT,
    WANT_RELOAD
};

// Режимы работы компилятора - определите ДО Config
enum class CompilerMode {
    COMPILE_ONLY,    // Только компиляция
    INTERPRET_ONLY,  // Только интерпретация
    HYBRID          // Компиляция + интерпретация для REPL
};

class Compiler {
public:
   struct CompilationOptions {
        std::string filename;
        std::string disassembly_output_file;
        bool load = false;
        bool color = false;
        bool write = false;
        bool no_code = false;
        bool disassemble = false;
        bool disasm_code_only = false;
        bool print_time = false;
        CompilerMode mode = CompilerMode::HYBRID;
        bool debug_print_ir = false;
        bool debug_print_ast = false;
        bool debug_print_asm = false;
        std::string user_profile = "#f";
        std::vector<std::string> search_paths;
    };

    // Исправьте конструктор - уберите дефолтное значение
    Compiler(SootPlatform platform,
           const CompilationOptions comp_options,
           const std::optional<REPL::Config> repl_config = {},
           const std::string& user_profile = "#f",
           std::unique_ptr<REPL::Wrapper> repl = nullptr);
    ~Compiler();

    // ========== Компиляция ==========
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(const std::filesystem::path& path);
    
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(soot::Object& forms, const std::string& filename);

    // ========== Интерпретация ==========
    [[nodiscard]] soot::Object interpret(const std::string& code);
    [[nodiscard]] soot::Object interpret(const soot::Object& forms);
    
    void save_repl_history();
    void print_to_repl(const std::string& str);
    std::string get_prompt();
    std::string get_repl_input();

    // ========== REPL ==========
    ReplStatus handle_repl_command(const std::string& cmd);
    ReplStatus interpret_and_print(const std::string& code);
    ReplStatus compile_and_report(const std::string& code);
    ReplStatus try_interpret_then_compile(const std::string& code);
    ReplStatus handle_repl_string(const std::string& input);

    // ========== Управление окружением ==========
    void set_global(const std::string& name, const soot::Object& value);
    soot::Object get_global(const std::string& name);
    void load_user_profile();
    void reload_environment();
    
    // ========== Настройки ==========
    TypeSystem& ts() { return m_ts; }
    CompilationOptions& config() { return m_config; }
    

private:
    // Компиляция в байт-код/бинарник
    std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_internal(soot::Object& forms, const std::string& filename);
    
    // Setup для REPL
    void setup_repl();
    void setup_goos_forms();
    
    // Поиск файлов
    std::string find_file(const std::string& filename);
    std::string read_file_content(const std::string& filename);
    
    // Color/Allocation (аналог их color_object_file)
    void color_binary_file(std::unique_ptr<BinaryFile>& binary);
    
    // Codegen (если нужен дополнительный проход)
    std::vector<uint8_t> codegen_binary(BinaryFile* binary);
    

    // ===============================================================
    // Interpretator
    // ===============================================================
    void setup_soot_forms();
    soot::Object builtin_get_enum_vals(const soot::Object& form, 
                                         soot::Arguments& args,
                                         const std::shared_ptr<soot::EnvironmentObject>& env);



    // ===============================================================
    // The arguments tools
    // ===============================================================
    soot::Arguments get_va(const soot::Object& form, const soot::Object& rest);
    soot::Arguments get_va_no_named(const soot::Object& form, const soot::Object& rest);
    void va_check(
        const soot::Object& form,
        const soot::Arguments& args,
        const std::vector<std::optional<soot::ObjectType>>& unnamed,
        const std::unordered_map<std::string, std::pair<bool, std::optional<soot::ObjectType>>>& named);
    void for_each_in_list(const soot::Object& list, const std::function<void(const soot::Object&)>& f);   

    // ===============================================================
    // Errors and Warnings
    // ===============================================================
    void print_error(const std::string& context, const std::exception& e);
    
    void print_warning(const std::string& warning);

    template <typename... Args>
    [[noreturn]] void throw_compiler_error(const soot::Object& code,
                                         const std::string& str,
                                         Args&&... args) {
        lg::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "-- Compilation Error! --\n");
        if (!str.empty() && str.back() == '\n') {
        lg::print(fmt::emphasis::bold, str, std::forward<Args>(args)...);
        } else {
        lg::print(fmt::emphasis::bold, str + '\n', std::forward<Args>(args)...);
        }

        lg::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Form:\n");
        lg::print("{}\n", code.print());
        throw CompilerException("Compilation Error");
    }

    template <typename... Args>
    [[noreturn]] void throw_compiler_error_no_code(const std::string& str, Args&&... args) {
        lg::print(fg(fmt::color::crimson) | fmt::emphasis::bold, "-- Compilation Error! --\n");
        if (!str.empty() && str.back() == '\n') {
        lg::print(fmt::emphasis::bold, str, std::forward<Args>(args)...);
        } else {
        lg::print(fmt::emphasis::bold, str + '\n', std::forward<Args>(args)...);
        }

        lg::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Form:\n");
        throw CompilerException("Compilation Error");
    }

    template <typename... Args>
    void print_compiler_warning(const std::string& str, Args&&... args) {
        lg::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "[Warning] ");
        if (!str.empty() && str.back() == '\n') {
        lg::print(str, std::forward<Args>(args)...);
        } else {
        lg::print(str + '\n', std::forward<Args>(args)...);
        }
    }

    // ===============================================================
    //  REPL Callbacks 
    // ===============================================================
    // Completion callback - ожидает возвращать vector<Replxx::Completion>
    replxx::Replxx::completions_t find_symbols_or_object_file_by_prefix(
        const std::string& context,
        int& context_len,
        const std::vector<std::string>& examples);

    replxx::Replxx::hints_t find_hints_by_prefix(
        const std::string& context,
        int& context_len,
        replxx::Replxx::Color& color,
        const std::vector<std::string>& examples);

    void repl_coloring(
        const std::string& input,
        replxx::Replxx::colors_t& colors);

    // ===============================================================
    // Fields
    // ===============================================================

    TypeSystem& m_ts;
    SootPlatform m_platform;
    CompilationOptions m_config;
    
    // Компоненты
    soot::Interpreter m_soot;
    MakeSystem m_make;
    std::unique_ptr<REPL::Wrapper> m_repl;
    std::unique_ptr<GlobalNode> m_global_env;
    std::unique_ptr<NoneNode> m_none;

    // Состояние
    bool m_want_exit = false;
    bool m_want_reload = false;
    std::string m_current_file;
    
    // Статистика
    struct DebugStats {
        int total_funcs = 0;
        int total_spills = 0;
        int funcs_requiring_v1_allocator = 0;
    } m_debug_stats;
};

} // namespace sootc