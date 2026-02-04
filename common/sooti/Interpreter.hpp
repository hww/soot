#pragma once

#include "common/sooti/Object.hpp"
#include "common/sooti/Reader.hpp"
#include "fmt/color.h"
#include "fmt/format.h"
#include <functional>
#include <memory>
#include <unordered_map>

namespace std {
template <> struct hash<script::InternedSymbolPtr> {
    size_t operator()(const script::InternedSymbolPtr &s) const noexcept {
        // Поскольку символы интернированы, адрес указателя
        // сам по себе является отличным уникальным хешем.
        return std::hash<const char *>{}(s.name_ptr);
    }
};
} // namespace std

class TypeSystem;

namespace script {
class ExitException : public std::exception {
  public:
    int exit_code;
    std::string message; // Храним строку здесь

    explicit ExitException(int code = 0)
        : exit_code(code), message(fmt::format("Exit with code {}", code)) {}

    const char *what() const noexcept override {
        return message.c_str(); // Теперь это безопасно
    }
};

enum class TypeSystemVariant {
    Undefined,
    Default,
    Z80,
};

class Interpreter {
    friend class SootTypeSystem;

    struct ContextFrame {
        int depth;
        Object form;
        ContextFrame *prev;

        std::string print() {
            return fmt::format("<ContextFrame depth={} form={} prev={:p}>", depth, form.print(),
                               static_cast<void *>(prev));
        }
    };

    struct FrameGuard {
        ContextFrame **top_frame_ptr;
        ContextFrame *old_frame;

        FrameGuard(ContextFrame **ptr, ContextFrame *new_val)
            : top_frame_ptr(ptr), old_frame(*ptr) {
            *top_frame_ptr = new_val;
        }

        ~FrameGuard() {
            *top_frame_ptr = old_frame;
        }

        std::string print() {
            ContextFrame *current = top_frame_ptr ? *top_frame_ptr : nullptr;
            return fmt::format("<FrameGuard current={:p} old={:p} ptr={:p}>",
                               static_cast<void *>(current), static_cast<void *>(old_frame),
                               static_cast<void *>(top_frame_ptr));
        }
    };

  public:
    Interpreter(const std::string &username = "user", bool load_libs = false);

    struct BuiltinEntryConfig {
        std::string name;
        BuiltinFormMethod method;
        ArgumentSpec *spec;
    };

    struct SpecialEntryConfig {
        std::string name;
        SpecialFormMethod method;
        ArgumentSpec *spec;
    };

    // --- Методы регистрации ---
    void add_special_form(std::string name, SpecialFormMethod method,
                          ArgumentSpec *specs = nullptr);
    void add_builtin_form(std::string name, BuiltinFormMethod method,
                          ArgumentSpec *specs = nullptr);

    // Основные методы оценки
    Object eval_string(const std::string &expression, const std::string &filename);

    Object eval_form(const Object &obj, const std::shared_ptr<EnvironmentObject> &env,
                     bool self_eval_place = true);

    // --- Доступ к приватным членам -------
    // Запуск REPL
    void execute_repl();
    // Лоступ к Reader
    Reader &get_reader() {
        return m_reader;
    }

    // Лоступ к окружению
    Object get_global_environment() {
        return m_global_environment;
    }
    TextDb &get_db() {
        return m_reader.get_db();
    }
    SymbolTable &symbol_table() {
        return m_symbol_table;
    }

    // --- Для REPL и LSP -------------------
    std::string get_all_symbols_matching(const std::string &prefix);

    // --- Константы ------------------------
    Object get_undefined() {
        return m_undefined;
    }
    Object get_null() {
        return m_null;
    }
    Object get_true() {
        return m_sym_true;
    }
    Object get_false() {
        return m_sym_false;
    }
    // Boolean helpers (используют символы)
    Object true_or_false(bool value) {
        return value ? m_sym_true : m_sym_false;
    }

    // --- Predicates -----------------------
    // Check if value is true
    bool truthy(const Object &o) const {
        return o.truthy(m_sym_false.as_symbol());
    }
    // Помощники для чисел
    bool is_number(const Object &obj);
    int64_t number_to_integer(const Object &obj);
    double number_to_float(const Object &obj);

  private:
    Object call_lambda_internal(const Object &lambda, const std::vector<Object> &args);
    Object eval_file_internal(const std::vector<std::string> &file_path);
    Object eval(const Object &parent_form, const Object &obj,
                const std::shared_ptr<EnvironmentObject> &env, bool self_eval_place = true);
    Object eval_with_rewind(const Object &parent_form, const Object &obj,
                            const std::shared_ptr<EnvironmentObject> &env,
                            bool self_eval_place = true);

    void eval_args(const Object &parent_form, Arguments *args,
                   const std::shared_ptr<EnvironmentObject> &env);

    void load_library();

    // Символы и окружение
    Object make_symbol(const char *name);
    Object make_symbol(const std::string &name);
    InternedSymbolPtr intern(const std::string &name);
    bool try_symbol_lookup(const Object &sym, const std::shared_ptr<EnvironmentObject> &env,
                           Object *dest);
    Object eval_symbol(const Object &parent_form, const Object &sym,
                       const std::shared_ptr<EnvironmentObject> &env);
    void define_var_in_env(const Object &env, const Object &var, const char *name);

    // Вспомогательные методы
    Arguments get_args(const Object &form, const Object &rest, const ArgumentSpec &spec);
    Arguments get_args_with_spec(const Object &form, const Object &rest, const ArgumentSpec &spec);
    Arguments get_args_no_named(const Object &form, const Object &rest, const ArgumentSpec &spec);

    std::vector<Object> eval_list(const Object &list,
                                  const std::shared_ptr<EnvironmentObject> &env);
    Object eval_list_return_last(const Object &form, Object rest,
                                 const std::shared_ptr<EnvironmentObject> &env);

    // Обработка ошибок
    void throw_eval_error(const Object &o, const std::string &err);
    void print_form_info(const Object &form);

  private:
    // === СПЕЦИАЛЬНЫЕ ФОРМЫ (не вычисляют аргументы) ===
    Object eval_quote_special(const Object &form, const Object &rest,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_define_special(const Object &form, const Object &rest,
                               const std::shared_ptr<EnvironmentObject> &env);
    Object eval_lambda_special(const Object &form, const Object &rest,
                               const std::shared_ptr<EnvironmentObject> &env);
    Object eval_begin_special(const Object &form, const Object &rest,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_set_special(const Object &form, const Object &rest,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object eval_let_special(const Object &form, const Object &rest,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object eval_if_special(const Object &form, const Object &rest,
                           const std::shared_ptr<EnvironmentObject> &env);
    Object eval_and_special(const Object &form, const Object &rest,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object eval_or_special(const Object &form, const Object &rest,
                           const std::shared_ptr<EnvironmentObject> &env);
    Object eval_cond_special(const Object &form, const Object &rest,
                             const std::shared_ptr<EnvironmentObject> &env);
    Object eval_while_special(const Object &form, const Object &rest,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_macro_special(const Object &form, const Object &rest,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_let_star_special(const Object &form, const Object &rest,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_quasiquote_special(const Object &form, const Object &rest,
                                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_apply(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    Object eval_macroexpand(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);

    // === ВСТРОЕННЫЕ ФУНКЦИИ (вычисляют аргументы) ===

    // Математические
    Object eval_plus(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_minus(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    Object eval_times(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    Object eval_divide(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_numequals(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_lt(const Object &form, Arguments &args,
                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_gt(const Object &form, Arguments &args,
                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_leq(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_geq(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);

    // Списки и пары
    Object eval_cons(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_car(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_cdr(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_list_func(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_length(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_append(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_null_p(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_pair_p(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);

    // Предикат дефиниции
    Object eval_bound_p(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);

    // Предикаты типов
    Object eval_symbol_p(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_number_p(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_integer_p(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_float_p(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_p(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_char_p(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_vector_p(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_procedure_p(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object eval_boolean_p(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_type_p(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_type_of(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);

    // Сравнение
    Object eval_equals(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);

    // Строки
    Object eval_string_length(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_ref(const Object &form, Arguments &args,
                           const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_append(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_substr(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_to_symbol(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_starts_with(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_ends_with(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_split(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_containsp(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_replace(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env);
    Object eval_symbol_to_string(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);

    // Векторы
    Object eval_vector(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_vector_ref(const Object &form, Arguments &args,
                           const std::shared_ptr<EnvironmentObject> &env);
    Object eval_vector_set(const Object &form, Arguments &args,
                           const std::shared_ptr<EnvironmentObject> &env);
    Object eval_vector_length(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_vector_to_list(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env);

    // Хэш-таблицы
    Object eval_make_hash_table(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_set(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_ref(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_p(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_try_ref(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_length(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_to_list(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_containsp(const Object &form, Arguments &args,
                                     const std::shared_ptr<EnvironmentObject> &env);

    // Итераторы
    Object eval_string_for_each(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env);
    Object eval_vector_for_each(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env);
    Object eval_hash_table_for_each(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_list_for_each(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);

    // Системные и ввод-вывод
    Object eval_print(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    Object eval_pfmt(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_inspect(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);
    Object eval_error(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    Object eval_fmt(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_cfmt(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);

    // Logger
    Object eval_log(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);

    // файлы
    Object eval_file_exists_p(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_read_str(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_parse_str(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_read_file(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_load(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);

    // Reader
    Object eval_set_macro_character(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_get_macro_character(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_remove_macro_character(const Object &form, Arguments &args,
                                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_read(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_read_char(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_peek_char(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_read_delimited_list(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_reader_p(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_cell_p(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_special_form_p(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env);
    Object eval_primitive_p(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);
    // Система
    Object eval_system(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_get_env(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);
    Object eval_exit(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_get_path(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_find_file(const Object &form, Arguments &args,
                          const std::shared_ptr<EnvironmentObject> &env);
    Object eval_write_binary_file(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env);
    Object eval_read_binary_file(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_write_text_file(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env);
    Object eval_read_text_file(const Object &form, Arguments &args,
                               const std::shared_ptr<EnvironmentObject> &env);
    Object eval_export_intel_hex(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_crc32(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    // Прочие
    Object eval_gensym(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_eval(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_set_car(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);
    Object eval_set_cdr(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);
    Object eval_defsetf(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);
    Object eval_get_setter(const Object &form, Arguments &args,
                           const std::shared_ptr<EnvironmentObject> &env);

    // Преобразования типов
    Object eval_number_to_string(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_string_to_number(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_char_to_integer(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env);
    Object eval_integer_to_char(const Object &form, Arguments &args,
                                const std::shared_ptr<EnvironmentObject> &env);

    // Математические функции
    Object eval_abs(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_max(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_min(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_expt(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_sqrt(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);
    Object eval_ash(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);

    // Bits
    Object eval_logand(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_logior(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_logxor(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_lognot(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_lshift(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);
    Object eval_rshift(const Object &form, Arguments &args,
                       const std::shared_ptr<EnvironmentObject> &env);

    // Математика и округление
    Object eval_floor(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    Object eval_ceiling(const Object &form, Arguments &args,
                        const std::shared_ptr<EnvironmentObject> &env);
    Object eval_round(const Object &form, Arguments &args,
                      const std::shared_ptr<EnvironmentObject> &env);
    Object eval_mod(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);

    // Тригонометрия
    Object eval_sin(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_cos(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_tan(const Object &form, Arguments &args,
                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_atan(const Object &form, Arguments &args,
                     const std::shared_ptr<EnvironmentObject> &env);

    // Константы
    Object eval_pi(const Object &form, Arguments &args,
                   const std::shared_ptr<EnvironmentObject> &env);

    // Время
    Object eval_time_seconds(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env);
    Object eval_time_milliseconds(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env);
    Object eval_time_microseconds(const Object &form, Arguments &args,
                                  const std::shared_ptr<EnvironmentObject> &env);
    Object eval_time_nanoseconds(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);

    // Quasiquote helpers
    Object quasiquote_helper(const Object &form, const std::shared_ptr<EnvironmentObject> &env);

    // Основной метод оценки пар
    Object eval_pair(const Object &parent_for, const Object &obj,
                     const std::shared_ptr<EnvironmentObject> &env);

    // Улучшенная обработка аргументов
    ArgumentSpec parse_arg_spec(const Object &form, Object &rest);
    void set_args_in_env(const Object &form, const Arguments &args, const ArgumentSpec &arg_spec,
                         const std::shared_ptr<EnvironmentObject> &env);
    void vararg_check(
        const Object &form, const Arguments &args,
        const std::vector<std::vector<ObjectType>> &unnamed,
        const std::unordered_map<std::string, std::pair<bool, std::vector<ObjectType>>> &named);

    Object eval_defenum_special(const Object &form, const Object &rest,
                                const std::shared_ptr<EnvironmentObject> &env);
    Object eval_deftype_special(const Object &form, const Object &rest,
                                const std::shared_ptr<EnvironmentObject> &env);
    Object eval_typespec_special(const Object &form, const Object &rest,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_types_to_lisp(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_init_types(const Object &form, Arguments &args,
                           const std::shared_ptr<EnvironmentObject> &env);

    Object eval_source_info(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object eval_get_context(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);

    Object eval_make_accessor(const Object &form, Arguments &args,
                              const std::shared_ptr<EnvironmentObject> &env);
    Object eval_navigation_special(const Object &form, const Object &rest,
                                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_make_buffer_cell(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_cell_get(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);
    Object eval_cell_set(const Object &form, Arguments &args,
                         const std::shared_ptr<EnvironmentObject> &env);

    Object eval_make_static_buffer(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_make_static_writer(const Object &form, Arguments &args,
                                   const std::shared_ptr<EnvironmentObject> &env);
    Object eval_buffer_write(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env);
    Object eval_static_writer_write(const Object &form, Arguments &args,
                                    const std::shared_ptr<EnvironmentObject> &env);
    Object eval_buffer_label_set(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_buffer_label_get(const Object &form, Arguments &args,
                                 const std::shared_ptr<EnvironmentObject> &env);
    Object eval_buffer_dump(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object eval_buffer_read(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object eval_buffer_reloc(const Object &form, Arguments &args,
                             const std::shared_ptr<EnvironmentObject> &env);
    Object eval_buffer_link(const Object &form, Arguments &args,
                            const std::shared_ptr<EnvironmentObject> &env);
    Object lookup_in_alist(Object list, const std::string &key);
    void recursive_write(Object cell_obj, Object value);

    // --- Инициализация системы типов ---
    void init_type_system(TypeSystemVariant types);
    // --- Инициализация Хранилища ---
    void init_special_forms(const std::initializer_list<SpecialEntryConfig> forms);
    void init_builtin_forms(const std::initializer_list<BuiltinEntryConfig> forms);
    // --- Хранилища ---

    // Типы и Сеттеры
    std::unordered_map<std::string, ObjectType> m_string_to_type;
    std::unordered_map<InternedSymbolPtr, InternedSymbolPtr> m_setter_map;

    // Состояние
    Object m_sym_true;
    Object m_sym_false;
    Object m_sym_undefined;
    const char *m_symbol_true;
    const char *m_symbol_false;
    const char *m_symbol_undefined;
    Object m_null;
    Object m_undefined;
    int m_gensym_id = 0;
    Object m_global_environment;
    Object m_comp_env;
    bool m_disable_printing = false;
    Reader m_reader;
    ContextFrame *m_top_frame;
    SymbolTable m_symbol_table;
};

fmt::terminal_color string_to_color(const std::string &name);
} // namespace script
