#include "reader.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <iostream>

// TokenStream implementation
TokenStream::TokenStream(std::shared_ptr<SourceText> source) 
    : m_source(std::move(source)) {}

char TokenStream::peek() {
    if (!has_more()) return '\0';
    return m_source->get_text()[m_position];
}

char TokenStream::peek(int ahead) {
    if (!has_more(ahead)) return '\0';
    return m_source->get_text()[m_position + ahead];
}

char TokenStream::read() {
    if (!has_more()) return '\0';
    
    char c = m_source->get_text()[m_position++];
    if (c == '\n') {
        m_line++;
    }
    return c;
}

bool TokenStream::has_more() const {
    return m_position < m_source->get_size();
}

bool TokenStream::has_more(int ahead) const {
    return m_position + ahead < m_source->get_size();
}

void TokenStream::skip_whitespace_and_comments() {
    while (has_more()) {
        char c = peek();
        
        if (std::isspace(c)) {
            read(); // consume whitespace
        } else if (c == ';') {
            // Skip comment until newline
            while (has_more() && read() != '\n') {}
        } else {
            break;
        }
    }
}

// Reader implementation
Reader::Reader() = default;

Object Reader::read_from_string(const std::string& code, const std::string& source_name) {
    auto source = std::make_shared<StringSource>(code, source_name);
    m_sources.register_source(source);
    return read_impl(std::move(source));
}

Object Reader::read_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    auto source = std::make_shared<FileSource>(filename, buffer.str());
    m_sources.register_source(source);
    return read_impl(std::move(source));
}

Object Reader::read_impl(std::shared_ptr<SourceText> source) {
    TokenStream tokens(std::move(source));
    tokens.skip_whitespace_and_comments();
    
    if (!tokens.has_more()) {
        return Object::make_empty_list(); // Empty input
    }
    
    Object result = read(tokens);
    
    // ДОБАВЬТЕ ЭТУ ПРОВЕРКУ: если остались токены - значит что-то не так
    tokens.skip_whitespace_and_comments();
    if (tokens.has_more()) {
        throw_reader_error(tokens, "Unexpected input after expression");
    }
    
    return result;
}

Object Reader::read(TokenStream& tokens) {
    Token token = next_token(tokens);
    
    if (token.text == "(") {
        return read_list(tokens);
    }
    else if (token.text == "'") {
        // Quote: 'expr -> (quote expr)
        Object quoted_expr = read(tokens);
        Object quote_symbol = m_symbols.intern("quote");
        link_object(quote_symbol, token);
        
        return Object::make_pair(quote_symbol, Object::make_pair(quoted_expr, Object::make_empty_list()));
    }
    else {
        return read_atom(token);
    }
}

Object Reader::read_list(TokenStream& tokens) {
    Token open_paren = next_token(tokens);
    
    std::vector<Object> elements;
    
    while (tokens.has_more()) {
        tokens.skip_whitespace_and_comments();
        
        if (!tokens.has_more()) {
            throw_reader_error(tokens, "Unclosed list - missing ')'");
        }
        
        if (tokens.peek() == ')') {
            tokens.read(); // consume ")"
            break;
        }
        
        Object element = read(tokens);
        std::cout << "DEBUG read_list: read element: " << element.print() << std::endl;
        elements.push_back(element);
    }
    
    std::cout << "DEBUG read_list: elements count: " << elements.size() << std::endl;
    for (size_t i = 0; i < elements.size(); i++) {
        std::cout << "  [" << i << "] = " << elements[i].print() << std::endl;
    }
    
    // Build proper list from elements
    Object result = Object::make_empty_list();
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        result = Object::make_pair(*it, result);
    }
    
    std::cout << "DEBUG read_list: final list: " << result.print() << std::endl;
    
    return result;
}
Token Reader::next_token(TokenStream& tokens) {
    tokens.skip_whitespace_and_comments();
    
    if (!tokens.has_more()) {
        throw std::runtime_error("Unexpected end of input");
    }
    
    Token token;
    token.source = tokens.get_source();
    token.offset = tokens.get_current_offset();
    token.line = tokens.get_current_line();
    
    char c = tokens.peek();
    
    if (c == '(' || c == ')' || c == '\'' || c == '`' || c == ',') {
        // Single character tokens
        token.text = std::string(1, tokens.read());
    }
    else if (c == '"') {
        // String literal
        tokens.read(); // consume opening quote
        std::string str;
        
        while (tokens.has_more() && tokens.peek() != '"') {
            if (tokens.peek() == '\\') {
                tokens.read(); // consume backslash
                if (!tokens.has_more()) {
                    throw_reader_error(tokens, "Unterminated escape sequence"); // ИЗМЕНИТЬ
                }
            }
            str += tokens.read();
        }
        
        if (!tokens.has_more()) {
            throw_reader_error(tokens, "Unterminated string literal"); // ИЗМЕНИТЬ
        }
        
        tokens.read(); // consume closing quote
        token.text = "\"" + str + "\"";
    }
    else {
        // Word (symbol, number, boolean, etc.)
        std::string word;
        while (tokens.has_more()) {
            char ch = tokens.peek();
            if (std::isspace(ch) || ch == '(' || ch == ')' || ch == ';' || ch == '"') {
                break;
            }
            word += tokens.read();
        }
        token.text = word;
    }
    
    return token;
}


Object Reader::read_atom(const Token& token) {
    Object result;
    
    // Check if it's a string first (ДОБАВЬТЕ ЭТОТ БЛОК)
    if (!token.text.empty() && token.text[0] == '"' && token.text[token.text.size()-1] == '"') {
        // Remove quotes and process escape sequences
        std::string content = token.text.substr(1, token.text.size() - 2);
        std::string processed;
        
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\\' && i + 1 < content.size()) {
                switch (content[i + 1]) {
                    case 'n': processed += '\n'; break;
                    case 't': processed += '\t'; break;
                    case 'r': processed += '\r'; break;
                    case '"': processed += '"'; break;
                    case '\\': processed += '\\'; break;
                    default: 
                        throw_reader_error(token, "Unknown escape sequence: \\" + std::string(1, content[i + 1]));
                }
                i++; // skip next character
            } else {
                processed += content[i];
            }
        }
        
        result = Object::make_string(processed);
        link_object(result, token);
        return result;
    }
    
    // Try different parsers in order of specificity
    if (try_parse_boolean(token, result) ||
        try_parse_char(token, result) ||
        try_parse_binary(token, result) ||
        try_parse_hex(token, result) ||
        try_parse_integer(token, result) ||
        try_parse_float(token, result)) {
        link_object(result, token);
        return result;
    }
    
    // Default to symbol
    result = m_symbols.intern(token.text);
    link_object(result, token);
    return result;
}

// Number parsers (like OpenGOAL)
bool Reader::try_parse_boolean(const Token& token, Object& result) {
    if (token.text == "#t" || token.text == "#T") {
        result = Object::make_boolean(true);
        return true;
    }
    else if (token.text == "#f" || token.text == "#F") {
        result = Object::make_boolean(false);
        return true;
    }
    return false;
}

bool Reader::try_parse_char(const Token& token, Object& result) {
    if (token.text.size() >= 3 && token.text[0] == '#' && token.text[1] == '\\') {
        if (token.text.size() == 3) {
            // Single character: #\a
            result = Object::make_char(token.text[2]);
            return true;
        }
        else if (token.text == "#\\space") {
            result = Object::make_char(' ');
            return true;
        }
        else if (token.text == "#\\newline") {
            result = Object::make_char('\n');
            return true;
        }
    }
    return false;
}

bool Reader::try_parse_binary(const Token& token, Object& result) {
    if (token.text.size() >= 3 && token.text[0] == '#' && token.text[1] == 'b') {
        // Check all characters are 0 or 1
        for (size_t i = 2; i < token.text.size(); i++) {
            if (token.text[i] != '0' && token.text[i] != '1') {
                return false;
            }
        }
        
        // Parse binary
        uint64_t value = 0;
        for (size_t i = 2; i < token.text.size(); i++) {
            if (value & (1ULL << 63)) {
                throw std::runtime_error("Binary constant overflow: " + token.text);
            }
            value = (value << 1) | (token.text[i] == '1' ? 1 : 0);
        }
        
        result = Object::make_integer(static_cast<IntType>(value));
        return true;
    }
    return false;
}

bool Reader::try_parse_hex(const Token& token, Object& result) {
    if (token.text.size() >= 3 && token.text[0] == '#' && token.text[1] == 'x') {
        // Check hex characters
        for (size_t i = 2; i < token.text.size(); i++) {
            char c = token.text[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return false;
            }
        }
        
        try {
            size_t pos = 0;
            uint64_t value = std::stoull(token.text.substr(2), &pos, 16);
            
            if (pos != token.text.size() - 2) {
                return false;
            }
            
            result = Object::make_integer(static_cast<IntType>(value));
            return true;
        }
        catch (const std::exception&) {
            throw std::runtime_error("Invalid hexadecimal constant: " + token.text);
        }
    }
    return false;
}

bool Reader::try_parse_integer(const Token& token, Object& result) {
    if (token.text.empty()) return false;
    
    // Check format: optional sign followed by digits only
    size_t start = (token.text[0] == '+' || token.text[0] == '-') ? 1 : 0;
    if (start >= token.text.size()) return false; // Just a sign
    
    for (size_t i = start; i < token.text.size(); i++) {
        if (!std::isdigit(token.text[i])) {
            return false;
        }
    }
    
    try {
        IntType value = std::stoll(token.text);
        result = Object::make_integer(value);
        return true;
    }
    catch (const std::exception&) {
        throw std::runtime_error("Integer constant out of range: " + token.text);
    }
}

bool Reader::try_parse_float(const Token& token, Object& result) {
    if (token.text.empty()) return false;
    
    // Check for scientific notation (replace d with e)
    std::string text = token.text;
    for (char& c : text) {
        if (c == 'd' || c == 'D') c = 'e';
    }
    
    // Check format: must contain . or e for float
    bool has_dot = text.find('.') != std::string::npos;
    bool has_e = text.find('e') != std::string::npos;
    
    if (!has_dot && !has_e) return false;
    
    // Check characters
    size_t start = (text[0] == '+' || text[0] == '-') ? 1 : 0;
    bool has_dot_seen = false;
    
    for (size_t i = start; i < text.size(); i++) {
        char c = text[i];
        if (c == '.') {
            if (has_dot_seen) return false; // Multiple dots
            has_dot_seen = true;
        }
        else if (c == 'e' || c == 'E') {
            // Exponent part - check remaining is valid
            if (i + 1 >= text.size()) return false;
            size_t exp_start = (text[i + 1] == '+' || text[i + 1] == '-') ? i + 2 : i + 1;
            for (size_t j = exp_start; j < text.size(); j++) {
                if (!std::isdigit(text[j])) return false;
            }
            break; // Rest of string validated
        }
        else if (!std::isdigit(c)) {
            return false;
        }
    }
    
    try {
        FloatType value = std::stod(text);
        result = Object::make_float(value);
        return true;
    }
    catch (const std::exception&) {
        throw std::runtime_error("Float constant out of range: " + token.text);
    }
}

void Reader::link_object(const Object& obj, const Token& token) {
    m_sources.link_object(obj, token.source, token.offset);
}


    void Reader::throw_reader_error(TokenStream& tokens, const std::string& error) {
        int offset = tokens.get_current_offset();
        auto source = tokens.get_source();
        
        auto [line, col] = source->get_line_and_column(offset);
        std::string line_text = source->get_line_containing_offset(offset);
        
        std::stringstream ss;
        ss << "Parse error at " << source->get_description() 
           << ":" << (line + 1) << ":" << (col + 1) << "\n";
        ss << "  " << line_text << "\n";
        ss << "  " << std::string(col, ' ') << "^\n";
        ss << error;
        
        throw std::runtime_error(ss.str());
    }
    
    void Reader::throw_reader_error(const Token& token, const std::string& error) {
        auto source = token.source;
        int offset = token.offset;
        
        auto [line, col] = source->get_line_and_column(offset);
        std::string line_text = source->get_line_containing_offset(offset);
        
        std::stringstream ss;
        ss << "Parse error at " << source->get_description() 
           << ":" << (line + 1) << ":" << (col + 1) << "\n";
        ss << "  " << line_text << "\n";
        ss << "  " << std::string(col, ' ') << "^\n";
        ss << error;
        
        throw std::runtime_error(ss.str());
    }

    bool Reader::is_input_complete(const std::string& code) {
    // Простая проверка: считаем скобки
    int paren_balance = 0;
    bool in_string = false;
    bool escape_next = false;
    
    for (char c : code) {
        if (escape_next) {
            escape_next = false;
            continue;
        }
        
        if (c == '\\') {
            escape_next = true;
            continue;
        }
        
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        
        if (!in_string) {
            if (c == '(') paren_balance++;
            else if (c == ')') paren_balance--;
        }
    }
    
    // Если есть незакрытые скобки или строки - ввод не завершен
    return paren_balance <= 0 && !in_string && !escape_next;
}