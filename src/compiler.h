#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "opcodes.h" 
#include <vector>
#include <cstdint>
#include <map>

// Структура инструкции твоей ВМ
struct Instruction {
    uint8_t opcode;
    uint8_t a;  // регистр назначения
    uint8_t b;  // источник 1
    uint8_t c;  // источник 2
    int32_t k;  // immediate значение
    
    Instruction(uint8_t op, uint8_t ra = 0, uint8_t rb = 0, uint8_t rc = 0, int32_t imm = 0)
        : opcode(op), a(ra), b(rb), c(rc), k(imm) {}
};

class Compiler {
    std::vector<Instruction> code;
    std::vector<int32_t> data;  // статические данные
    std::map<std::string, size_t> symbol_table;  // таблица символов
    
    size_t next_register;
    size_t next_data_index;
    
    // Вспомогательные методы
    size_t allocate_register() ;
    size_t add_string_constant(const std::string& str);
    size_t add_symbol_constant(const std::string& symbol);
    
    // Генерация кода для разных узлов
    size_t compile_expression(const ASTNode* node, size_t result_reg);
    size_t compile_list(const ASTNode* node, size_t result_reg);
    size_t compile_symbol(const ASTNode* node, size_t result_reg);
    size_t compile_number(const ASTNode* node, size_t result_reg);
    size_t compile_string(const ASTNode* node, size_t result_reg);
    size_t compile_call(const ASTNode* node, size_t result_reg);
    size_t compile_list_data(const ASTNode* node, size_t result_reg);
    void compile_error(const ASTNode* node, const std::string& message);

public:
    Compiler() : next_register(0), next_data_index(0) {}
    
    std::vector<Instruction> compile(const ASTNode* ast);
    void print_bytecode() const;
};

#endif