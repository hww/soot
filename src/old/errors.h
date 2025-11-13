#ifndef ERRORS_H
#define ERRORS_H

#include <stdexcept>
#include <string>
#include "source-info.h"

// Базовое исключение для ошибок компиляции
class CompileError : public std::exception {
    SourceInfo source_info;
    std::string message;
    
public:
    CompileError(const SourceInfo& info, const std::string& msg)
        : source_info(info), message(msg) {}
    
    const char* what() const noexcept override {
        return message.c_str();
    }
    
    // Форматирование ошибки с контекстом
    std::string format_error() const {
        return source_info.format_error(message);
    }
    
    const SourceInfo& get_source_info() const { return source_info; }
};

// Специализированные исключения для разных этапов
class TokenizeError : public CompileError {
public:
    TokenizeError(const SourceInfo& info, const std::string& msg)
        : CompileError(info, msg) {}
};

class ParseError : public CompileError {
public:
    ParseError(const SourceInfo& info, const std::string& msg)
        : CompileError(info, msg) {}
};

class CompilationError : public CompileError {
public:
    CompilationError(const SourceInfo& info, const std::string& msg)
        : CompileError(info, msg) {}
};

#endif