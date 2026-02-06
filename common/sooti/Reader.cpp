#include "Reader.hpp"
#include "Interpreter.hpp"
#include "ListBuilder.hpp"
#include "Printer.hpp"
#include "common/util/FileUtil.hpp" // твой file_hub
#include "fmt/format.h"
#include "fmt/ranges.h"
// #include "oaidl.h"
#include <cctype>

namespace script {
namespace {
/*!
 * Is this a valid character to start a decimal integer number?
 */
bool decimal_start(char c) {
    return (c >= '0' && c <= '9') || c == '-';
}

/*!
 * Is this a valid character to start a floating point number?
 */
bool float_start(char c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '.';
}

/*!
 * Is this a valid character for a hex number?
 */
bool hex_char(char c) {
    return !((c < '0' || c > '9') && (c < 'a' || c > 'f') && (c < 'A' || c > 'F'));
}

/*!
 * Does the given string contain c?
 */
bool str_contains(const std::string &str, char c) {
    for (auto &x : str) {
        if (x == c) {
            return true;
        }
    }
    return false;
}

bool is_printable_char(char c) {
    return c >= ' ' && c <= '~';
}
} // namespace

// ==================== TextStream ====================

/*!
 * Advance a TextStream through any comments or whitespace.
 * This will leave the stream at the next non-whitespace character (or at the end)
 */
void TextStream::seek_past_whitespace_and_comments() {
    while (text_remains()) {
        char c = peek();
        switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            read();
            break;

        case ';':
            while (text_remains() && read() != '\n') {
            }
            break;

        case '#':
            if (text_remains(1) && peek(1) == '|') {
                ASSERT(read() == '#'); // #
                ASSERT(read() == '|'); // |

                bool found_end = false;
                while (text_remains() && !found_end) {
                    while (text_remains() && read() != '|') {
                    }
                    if (text_remains() && read() == '#') {
                        found_end = true;
                    }
                }
                continue;
            } else {
                return;
            }
            break;

        default:
            return;
        }
    }
}

/*!
 * Read encoding bytes on a TextStream and check if it's UTF-8.
 * If it's not, you can choose to throw or not.
 * If UTF-8 encoding is not detected, the stream is not advanced.
 */
void TextStream::read_utf8_encoding(bool throw_on_error) {
    if (text_remains(2)) {
        if ((uint8_t)peek(0) == 0xEF && (uint8_t)peek(1) == 0xBB && (uint8_t)peek(2) == 0xBF) {
            read();
            read();
            read();
            return;
        }
    }

    if (throw_on_error) {
        throw std::runtime_error(
            fmt::format("UTF-8 encoding not detected in {}", text->get_description()));
    }
}

// ==================== Reader ====================

Reader::Reader(Interpreter *interpeter) : m_interpreter(interpeter) {
    // add default macros
    add_reader_macro("'", "quote");
    add_reader_macro("`", "quasiquote");
    add_reader_macro(",", "unquote");
    add_reader_macro(",@", "unquote-splicing");

    // setup table of which characters are valid for starting a symbol
    for (auto &x : m_valid_symbols_chars) {
        x = false;
    }

    for (char x = 'a'; x <= 'z'; x++) {
        m_valid_symbols_chars[(int)x] = true;
    }

    for (char x = 'A'; x <= 'Z'; x++) {
        m_valid_symbols_chars[(int)x] = true;
    }

    for (char x = '0'; x <= '9'; x++) {
        m_valid_symbols_chars[(int)x] = true;
    }

    const char bonus[] = "!$%&*+-/\\.,@^_-;:<>?~=#";
    for (const char *c = bonus; *c; c++) {
        m_valid_symbols_chars[(int)*c] = true;
    }
}

/*!
 * Read a stream. Предназначен исключительно для character-macro функционирования
 */
Object Reader::read_single_form(TextStream &ts, EvalCallback eval_callback) {
    ts.seek_past_whitespace_and_comments();
    if (!ts.text_remains())
        return Object::make_null();

    auto tok = get_next_token(ts);
    auto it = m_reader_macros.find(tok.text);

    // --- ПУТЬ 1: МАКРОСЫ ---
    if (it != m_reader_macros.end()) {
        const auto &macro = it->second;

        // А) Функциональный макрос (Лямбда)
        if (macro.lambda.is_lambda()) {
            if (eval_callback) {
                ReaderEvent evt = {
                    type : ReaderEvent::Type::MACRO_REQUEST,
                    form : macro.lambda,
                    reader : Object::make_reader(&ts), // Поток СРАЗУ после знака макроса
                    token : Object::make_string(macro.shortcut)
                };
                return eval_callback(evt);
            }
            // Если коллбэка нет, мы не можем запустить лямбду — это ошибка конфигурации
            throw_reader_error(ts, "Execution callback required for functional macro: " + tok.text,
                               0);
        }

        // Б) Простой префиксный макрос (например, ' -> quote)
        if (!macro.replacement.empty()) {
            // Читаем ТО, ЧТО ИДЕТ СЛЕДОМ за макросом (рекурсивно)
            Object inner_obj = read_single_form(ts, eval_callback);

            Object sym = Object::make_symbol(macro.replacement.c_str());
            // Возвращаем либо (quote объект), либо просто символ замены
            return macro.list ? script::build_list({sym, inner_obj}) : sym;
        }
    }

    // --- ПУТЬ 2: ОБЫЧНЫЕ ОБЪЕКТЫ (Если это не макрос) ---
    Object result;
    if (tok.text == "(") {
        return read_list(ts, true, ")", eval_callback);
    } else if (tok.text == "[") {
        return read_list(ts, true, "]", eval_callback);
    } else {
        if (read_object(tok, ts, result)) {
            return result;
        }
        throw_reader_error(ts, "Invalid token: " + tok.text, -int(tok.text.size()));
    }
    return Object::make_null();
}

/*!
 * Read a string.
 */
Object Reader::read_from_string(const std::string &str, bool add_top_level,
                                const std::optional<std::string> &string_name,
                                EvalCallback                      eval_callback) {
    auto textFrag = std::make_shared<ProgramString>(str, string_name.value_or("Program string"));
    m_db.insert(textFrag);

    auto result = internal_read(textFrag, false, add_top_level, eval_callback);
    m_db.link(result, textFrag, 0);
    return result;
}
/*!
 * Read a file.
 */
Object Reader::read_from_file(const std::vector<std::string> &file_path, bool check_encoding,
                              bool add_top_level, EvalCallback eval_callback) {
    std::string file_descriptor = fmt::format("{}", fmt::join(file_path, "/"));
    const auto  joined_file_path = file_util::get_file_path(file_path);

    if (!file_util::exists(joined_file_path)) {
        throw std::runtime_error(
            fmt::format("Cannot read {}, file doesn't exist", joined_file_path));
    }

    auto textFrag = std::make_shared<FileText>(joined_file_path, file_descriptor);
    m_db.insert(textFrag);

    auto result = internal_read(textFrag, check_encoding, add_top_level, eval_callback);
    m_db.link(result, textFrag, 0);
    return result;
}

/*!
 * Common read for a SourceText
 */
Object Reader::internal_read(std::shared_ptr<SourceText> text, bool check_encoding,
                             bool add_top_level, EvalCallback eval_callback) {
    if (check_encoding &&
        (text->get_size() < 3 || (uint8_t)text->get_text()[0] != 0xEF ||
         (uint8_t)text->get_text()[1] != 0xBB || (uint8_t)text->get_text()[2] != 0xBF)) {
        throw std::runtime_error(
            fmt::format("Text file {} has invalid encoding", text->get_description()));
    }

    // first create stream
    TextStream ts(text);

    if (check_encoding) {
        // discard the UTF-8 encoding bytes
        ts.read_utf8_encoding(true);
    }
    // clean up first whitespace
    ts.seek_past_whitespace_and_comments();

    // read list!
    ListBuilder full_program_builder;
    auto        empty_list = Object::make_null();
    Object      eval_result = empty_list;

    try {

        while (true) {
            ts.seek_past_whitespace_and_comments();
            if (!ts.text_remains())
                break;

            // Читаем ровно одну форму (атом или список)
            // Твой read_from_stream уже умеет это делать
            Object form = read_single_form(ts, eval_callback);

            // Если передан делегат (например, из load), исполняем форму СРАЗУ.
            // Это позволяет макросам, определенным в начале файла,
            // работать для кода в конце того же файла.
            if (eval_callback) {
                ReaderEvent evt;
                evt.type = ReaderEvent::FORM_READ;
                evt.form = form;
                evt.reader = empty_list;
                evt.token = empty_list;

                eval_result = eval_callback(evt);
            }

            full_program_builder.push_back(std::move(form));
        }
    } catch (const std::exception &e) {
        // Здесь можно добавить логику восстановления после ошибки,
        // чтобы прочитать остаток файла, если нужно.
        throw;
    }

    auto result = full_program_builder.build();

    if (add_top_level) {
        return Object::make_pair(Object::make_symbol("top-level"), result);
    }
    return result;
}

Object Reader::read_one(TextStream &ts) {
    ts.seek_past_whitespace_and_comments();
    if (!ts.text_remains())
        return Object::make_null();

    Token  tok = get_next_token(ts);
    Object obj;

    // Смотрим первый символ
    char c = ts.peek();
    if (c == '(') {
        // Список
        ts.read(); // съедаем '('
        return read_list(ts, true, ")");
    } else if (c == '[') {
        // Список
        ts.read(); // съедаем '['
        return read_list(ts, true, "]");
    } else {
        // Одиночный токен
        auto   tok = get_next_token(ts);
        Object obj;
        read_object(tok, ts, obj);
        return obj;
    }
    return Object::make_null();
}

// ==================== Token Reading ====================

/*!
 * Given a stream starting at the first character of a token, get the token. Doesn't consume
 * whitespace at the end and leaves the stream on the first character after the token.
 */
Token Reader::get_next_token(TextStream &stream) {
    stream.seek_past_whitespace_and_comments();

    if (!stream.text_remains())
        return {};

    Token t;
    t.source_line = stream.line_count;
    t.source_offset = stream.seek;
    t.source_text = stream.text;

    char first = stream.read();
    t.text.push_back(first);

    // --- НОВОЕ: Обработка литерала символа #\ ---
    if (first == '#' && stream.text_remains() && stream.peek() == '\\') {
        t.text.push_back(stream.read()); // Добавляем '\'

        // Читаем ПЕРВЫЙ символ после косой черты обязательно
        if (stream.text_remains()) {
            t.text.push_back(stream.read());
        }

        // Продолжаем читать, пока идут буквы/цифры (для имен типа newline, space)
        // Но останавливаемся перед пробелом или разделителями (скбоками)
        while (stream.text_remains()) {
            char next = stream.peek();
            if (isspace(next) || next == ')' || next == '(' || next == '[' || next == ']' ||
                next == '"' || next == ';') {
                break;
            }
            t.text.push_back(stream.read());
        }
        return t;
    }

    // 1. Приоритет №1: Составные токены (например, ,@)
    if (first == ',' && stream.text_remains() && stream.peek() == '@') {
        t.text.push_back(stream.read());
        return t;
    }

    // 2. Встроенные разделители (теперь без конфликта с символами)
    if (first == '(' || first == ')' || first == '"' || first == '\'' || first == '`') {
        return t;
    }

    // 3. Reader Macro символы
    if (m_reader_macros.find(std::string(1, first)) != m_reader_macros.end()) {
        return t;
    }

    // 4. Чтение обычного токена
    while (stream.text_remains()) {
        char next = stream.peek();

        // Проверяем на макрос-разделитель
        if (isspace(next) || next == ')' || next == '(' || next == '"' || next == ';' ||
            m_reader_macros.find(std::string(1, next)) != m_reader_macros.end()) {
            break;
        } else {
            t.text.push_back(stream.read());
        }
    }

    return t;
}

// ==================== Token Reading Macro ====================

/*!
 * Add a macro that replaces the sequence of [shortcut, other_token] with
 * (replacement other_token) <- a list with two objects, replacement is a symbol.
 * These are used to make 'x turn into (quote x) and similar.
 */
void Reader::add_reader_macro(const std::string &shortcut, std::string replacement, bool list) {
    ReaderMacro m;
    m.shortcut = shortcut;
    m.replacement = std::move(replacement);
    m.list = list;
    m.lambda = Object::make_null();
    m_reader_macros[shortcut] = m;
}

void Reader::add_reader_macro(const std::string &shortcut, Object lambda, bool list) {
    ReaderMacro m;
    m.shortcut = shortcut;
    m.lambda = std::move(lambda);
    m.replacement = ""; // В данном случае приоритет у лямбды
    m.list = list;

    m_reader_macros[shortcut] = m;
}

void Reader::remove_reader_macro(const std::string &shortcut) {
    auto it = m_reader_macros.find(shortcut);
    if (it != m_reader_macros.end()) {
        m_reader_macros.erase(it);
        fmt::print("[READER] Removed macro: '{}'\n", shortcut);
    } else {
        fmt::print("[READER] Macro '{}' not found\n", shortcut);
    }
}

// Возвращаем указатель или опционал, чтобы избежать копирования и ошибок доступа
const Reader::ReaderMacro *Reader::find_reader_macro(const std::string &shortcut) const {
    auto it = m_reader_macros.find(shortcut);
    if (it != m_reader_macros.end()) {
        return &it->second;
    }
    return nullptr;
}

// ==================== Object Reading ====================

bool Reader::read_object(Token &tok, TextStream &ts, Object &obj) {
    try {
        if (try_token_as_integer(tok, obj))
            return true;
        if (try_token_as_hex(tok, obj))
            return true;
        if (try_token_as_binary(tok, obj))
            return true;
        if (try_token_as_float(tok, obj))
            return true;

        // try as string
        if (tok.text[0] == '"') {
            ASSERT(tok.text.length() == 1);
            if (read_string(ts, obj))
                return true;
            throw_reader_error(ts, "failed to read string, close quote not found", -1);
            return false;
        }

        if (tok.text[0] == '#' && tok.text.size() >= 2 && tok.text[1] == '(') {
            if (read_array(ts, obj))
                return true;
        }

        if (try_token_as_char(tok, obj))
            return true;
        if (try_token_as_symbol(tok, obj))
            return true;

    } catch (std::exception &e) {
        throw_reader_error(ts, "parsing token " + tok.text + " failed: " + e.what(), -1);
    }

    return false;
}

bool Reader::read_array(TextStream &stream, Object &o) {
    stream.seek_past_whitespace_and_comments();
    std::vector<Object> objects;

    bool got_close_paren = false;
    while (stream.text_remains()) {
        auto tok = get_next_token(stream);
        ASSERT(!tok.text.empty());

        if (tok.text[0] == '(') {
            ASSERT(tok.text.length() == 1);
            objects.push_back(read_list(stream, true));
            stream.seek_past_whitespace_and_comments();
            continue;
        } else if (tok.text[0] == ')') {
            ASSERT(tok.text.length() == 1);
            got_close_paren = true;
            break;
        } else {
            Object next_obj;
            if (read_object(tok, stream, next_obj)) {
                stream.seek_past_whitespace_and_comments();
                objects.push_back(next_obj);
            } else {
                throw_reader_error(stream, "invalid token encountered in array reader: " + tok.text,
                                   -int(tok.text.size()));
            }
        }
    }

    if (!got_close_paren) {
        throw_reader_error(stream, "An array must end in a close parenthesis", -1);
        return false;
    }

    o = Object::make_array(objects);
    return true;
}

bool Reader::try_token_as_symbol(const Token &tok, Object &obj) {
    if (tok.text.empty())
        return false;

    // Проверяем, начинается ли токен с ':'
    if (tok.text[0] == ':') {
        // Ключевые слова должны содержать хотя бы один символ после ':'
        if (tok.text.size() == 1)
            return false;

        // Проверяем остальные символы на валидность
        for (size_t i = 1; i < tok.text.size(); i++) {
            char c = tok.text[i];
            if (!m_valid_symbols_chars[(int)c]) {
                return false;
            }
        }

        // Создаем ключевое слово (убираем ведущий ':')
        obj = Object::make_keyword(tok.text.c_str());
        return true;
    }

    char first = tok.text[0];
    if (!m_valid_symbols_chars[(int)first]) {
        return false;
    }

    for (size_t i = 1; i < tok.text.size(); i++) {
        char c = tok.text[i];
        if (!m_valid_symbols_chars[(int)c]) {
            return false;
        }
    }

    obj = Object::make_symbol(tok.text.c_str());
    return true;
}
// ==================== List Reading ====================

/*!
 * Call this on the character after the open paren.
 */
Object Reader::read_list(TextStream &ts, bool expect_close_paren, std::string terminator,
                         EvalCallback eval_callback) {
    ts.seek_past_whitespace_and_comments();
    ListBuilder list_builder;
    int         start_offset = ts.seek;

    bool got_dot = false;
    bool got_thing_after_dot = false;
    bool got_terminator = false;

    while (ts.text_remains()) {
        ts.seek_past_whitespace_and_comments();
        int last_seek = ts.seek;

        auto tok = get_next_token(ts);
        if (tok.text.empty())
            break;

        // 1. Проверка терминатора
        if (tok.text == terminator) {
            got_terminator = true;
            break;
        }

        // 2. Обработка точки
        if (tok.text == ".") {
            if (got_dot)
                throw_reader_error(ts, "Multiple dots in list", -1);
            if (list_builder.size == 0)
                throw_reader_error(ts, "List cannot start with dot", -1);
            got_dot = true;
            continue;
        }

        // 3. ЧТЕНИЕ ОБЪЕКТА
        // Откатываемся, чтобы read_single_form прочитала токен сама (с учетом макросов)
        ts.seek = last_seek;
        Object current_obj = read_single_form(ts, eval_callback);

        // 4. ВСТАВКА В СПИСОК
        if (got_dot) {
            if (got_thing_after_dot)
                throw_reader_error(ts, "Only one object allowed after dot", -1);

            // Ручное "пришивание" хвоста к CDR последней созданной Pair
            if (list_builder.tail) {
                list_builder.tail->cdr = current_obj;
            }
            got_thing_after_dot = true;
        } else {
            // Обычный push_back
            list_builder.push_back(current_obj);

            // Линковка текущей ячейки Pair для БД
            if (list_builder.tail) {
                Object p;
                p.type = ObjectType::PAIR;
                p.heap_obj = list_builder.tail;
                m_db.link(p, ts.text, last_seek);
            }
        }

        ts.seek_past_whitespace_and_comments();
    }

    // Финальная проверка на закрывающую скобку
    if (expect_close_paren && !got_terminator) {
        throw_reader_error(ts, "Unclosed list: expected '" + terminator + "'", 0);
    }

    list_builder.build();
    // Линкуем весь список целиком
    m_db.link(list_builder.head, ts.text, start_offset);

    return list_builder.head;
}
/*!
 * Read a string and escape. Start on the first char after the first double quote.
 * Supported escapes are \n, \t, \\ and work like they do in C.
 * An arbitrary character can be entered as \c12 where the "12" is hexadecimal.
 */
bool Reader::read_string(TextStream &stream, Object &obj) {
    bool        got_close_quote = false;
    std::string str;

    while (stream.text_remains()) {
        char c = stream.read();
        if (c == '"') {
            obj = Object::make_string(str);
            got_close_quote = true;
            break;
        }

        if (c == '\\') {
            if (!stream.text_remains()) {
                throw_reader_error(stream, "incomplete string escape code", -1);
            }
            if (stream.peek() == 'n') {
                stream.read();
                str.push_back('\n');
            } else if (stream.peek() == 't') {
                stream.read();
                str.push_back('\t');
            } else if (stream.peek() == '\\') {
                stream.read();
                str.push_back('\\');
            } else if (stream.peek() == '"') {
                stream.read();
                str.push_back('"');
            } else if (stream.peek() == 'c') {
                stream.read();
                if (!stream.text_remains(2)) {
                    throw_reader_error(stream, "incomplete string escape code", -1);
                }
                auto first = stream.read();
                auto second = stream.read();
                if (!hex_char(first) || !hex_char(second)) {
                    throw_reader_error(stream, "invalid character escape hex number", -3);
                }
                char hex_num[3] = {first, second, '\0'};
                auto value = std::stoul(hex_num, nullptr, 16);
                if (value >= 256) {
                    throw_reader_error(stream, "invalid character escape", -2);
                }
                ASSERT(value < 256);
                str.push_back(char(value));
            } else {
                throw_reader_error(stream, "unknown string escape code", -1);
            }
        } else {
            str.push_back(c);
        }
    }

    return got_close_quote;
}

// ==================== Number Parsers ====================

/*!
 * Try decoding as a float.  Must have a "." in it.
 * Otherwise all combinations of leading zeros, "."'s, negative signs, etc are ok.
 * Trailing zeros not required.
 */
bool Reader::try_token_as_float(const Token &tok, Object &obj) {
    if (float_start(tok.text[0]) && str_contains(tok.text, '.')) {
        size_t offset = tok.text[0] == '-' ? 1 : 0;
        for (; offset < tok.text.size(); offset++) {
            char c = tok.text.at(offset);
            if ((c < '0' || c > '9') && (c != '.')) {
                return false;
            }
        }

        try {
            std::size_t end = 0;
            double      v = std::stod(tok.text, &end);
            if (end != tok.text.size())
                return false;
            obj = Object::make_float(v);
            return true;
        } catch (std::exception &e) {
            return false;
        }
    }
    return false;
}

/*!
 * Try decoding as binary. Looks like #b101010 ...
 * 64-bit unsigned
 */
bool Reader::try_token_as_binary(const Token &tok, Object &obj) {
    if (tok.text.size() >= 3 && tok.text[0] == '#' && tok.text[1] == 'b') {
        for (size_t offset = 2; offset < tok.text.size(); offset++) {
            char c = tok.text.at(offset);
            if (c != '0' && c != '1') {
                return false;
            }
        }

        uint64_t value = 0;

        for (uint32_t i = 2; i < tok.text.size(); i++) {
            if (value & (0x8000000000000000)) {
                throw std::runtime_error("overflow in binary constant: " + tok.text);
            }

            value <<= 1u;
            if (tok.text[i] == '1') {
                value++;
            } else if (tok.text[i] != '0') {
                return false;
            }
        }
        obj = Object::make_integer((int64_t)value);
        return true;
    }
    return false;
}
bool Reader::try_token_as_hex(const Token &tok, Object &obj) {
    if (tok.text.size() >= 3 && tok.text[0] == '#' && tok.text[1] == 'x') {
        // determine if we look like a number or not. If we look like a number, but stoll fails,
        // it means that the number is too big or too small, and we should error
        for (size_t offset = 2; offset < tok.text.size(); offset++) {
            char c = tok.text.at(offset);
            if (!hex_char(c)) {
                return false;
            }
        }

        uint64_t v = 0;
        try {
            std::size_t end = 0;
            v = std::stoull(tok.text.substr(2), &end, 16);
            if (end + 2 != tok.text.size())
                return false;
            obj = Object::make_integer(v);
            return true;
        } catch (std::exception &e) {
            throw std::runtime_error("The number " + tok.text +
                                     " cannot be a hexadecimal constant");
        }
    }
    return false;
}
/*!
 * Try decoding as integer. No decimals points allowed.
 * 64-bit signed. Won't accept values between INT64_MAX and UINT64_MAX.
 */
bool Reader::try_token_as_integer(const Token &tok, Object &obj) {
    if (decimal_start(tok.text[0]) && !str_contains(tok.text, '.')) {
        // determine if we look like a number or not. If we look like a number, but stoll fails,
        // it means that the number is too big or too small, and we should error
        size_t offset = tok.text[0] == '-' ? 1 : 0;
        if (offset == 1 && tok.text.size() == 1) {
            return false; // - by itself is not a number!
        }
        for (; offset < tok.text.size(); offset++) {
            char c = tok.text.at(offset);
            if (c < '0' || c > '9') {
                return false;
            }
        }
        uint64_t v = 0;
        try {
            std::size_t end = 0;
            v = std::stoll(tok.text, &end);
            if (end != tok.text.size()) {
                return false;
            }
            obj = Object::make_integer(v);
            return true;
        } catch (std::exception &e) {
            throw std::runtime_error("The number " + tok.text + " cannot be an integer constant");
        }
    }
    return false;
}

bool Reader::try_token_as_char(const Token &tok, Object &obj) {
    if (tok.text.size() >= 3 && tok.text[0] == '#' && tok.text[1] == '\\') {
        if (tok.text.size() == 3 && is_printable_char(tok.text[2]) && tok.text[2] != ' ') {
            obj = Object::make_char(tok.text[2]);
            return true;
        }

        if (tok.text.size() == 4 && tok.text[2] == '\\') {
            switch (tok.text[3]) {
            case 'n':
                obj = Object::make_char('\n');
                return true;
            case 's':
                obj = Object::make_char(' ');
                return true;
            case 't':
                obj = Object::make_char('\t');
                return true;
            }
        }
    }
    return false;
}

// ==================== Error Handling ====================

/*!
 * Throw an exception with useful information because of an error in the text stream.
 * Used for reader errors, like "missing close paren" or similar.
 */
void Reader::throw_reader_error(TextStream &here, const std::string &err, int seek_offset) {
    throw std::runtime_error(err + "\n" + // Убрали "at "
                             m_db.get_info_for(here.text, here.seek + seek_offset));
}
/*!
 * Convert any string into one that can be read.
 * Unprintable characters become escape sequences, including tab and newline.
 */
std::string get_readable_string(const char *in) {
    std::string result;
    while (*in) {
        if (is_printable_char(*in) && *in != '\\' && *in != '"') {
            result.push_back(*in);
        } else if (*in == '\n') {
            result += "\\n";
        } else if (*in == '\t') {
            result += "\\t";
        } else if (*in == '\\') {
            result += "\\\\";
        } else if (*in == '"') {
            result += "\\\"";
        } else {
            result += fmt::format("\\c{:02x}", uint8_t(*in));
        }
        in++;
    }
    return result;
}

std::string get_byte_string(const char *in) {
    std::string result;
    while (*in) {
        result += fmt::format("\\c{:02x}", uint8_t(*in));
        in++;
    }
    return result;
}

// ================== Test completitopn ===================
//
// Реализация в reader.cpp
bool Reader::is_expression_complete(const std::string &code) {
    auto       text = std::make_shared<ReplText>(code);
    TextStream ts(text);

    try {
        return is_expression_complete_impl(ts);
    } catch (...) {
        // Если произошла ошибка парсинга, считаем выражение неполным
        return false;
    }
}

bool Reader::is_expression_complete_impl(TextStream &ts) {
    int  paren_balance = 0;
    int  bracket_balance = 0;
    int  brace_balance = 0;
    bool in_string = false;
    bool in_comment = false;
    bool escape_next = false;

    while (ts.text_remains()) {
        char c = ts.read();
        char next = ts.text_remains() ? ts.peek() : '\0';

        if (escape_next) {
            escape_next = false;
            continue;
        }

        if (in_comment) {
            if (c == '\n') {
                in_comment = false;
            }
            continue;
        }

        if (in_string) {
            if (c == '\\') {
                escape_next = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }

        switch (c) {
        case '#':
            if (next == '\\') {
                ts.read(); // Съедаем саму обратную косую черту '\'
                if (ts.text_remains()) {
                    ts.read(); // Съедаем любой символ, который идет следом (например, '[')
                }
            }
            break;
        case ';':
            in_comment = true;
            break;
        case '"':
            in_string = true;
            break;
        case '(':
            paren_balance++;
            break;
        case ')':
            paren_balance--;
            break;
        case '[':
            bracket_balance++;
            break;
        case ']':
            bracket_balance--;
            break;
        case '{':
            brace_balance++;
            break;
        case '}':
            brace_balance--;
            break;
        }

        // Если баланс скобок стал отрицательным - ошибка
        if (paren_balance < 0 || bracket_balance < 0 || brace_balance < 0) {
            return true; // Завершено, но с ошибкой
        }
    }

    // Выражение завершено, если все скобки сбалансированы и мы не в строке/комментарии
    return paren_balance == 0 && bracket_balance == 0 && brace_balance == 0 && !in_string;
}
} // namespace script
