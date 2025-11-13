#include "compiler.h"
#include "crc32.h"
#include "source-info.h"

#include <iostream>
#include <sstream>
#include <map>  // Добавляем этот include
#include "compiler.h"

size_t Compiler::add_string_constant(const std::string& str) {
    uint32_t crc = compute_crc32(str);
    data.push_back(static_cast<int32_t>(crc));
    return next_data_index++;
}

size_t Compiler::add_symbol_constant(const std::string& symbol) {
    return add_string_constant(symbol);
}

size_t Compiler::compile_symbol(const ASTNode* node, size_t result_reg) {
    std::string symbol_name(node->token.text);
    
    // Для теста - просто загружаем LookupPointer
    size_t data_index = add_symbol_constant(symbol_name);
    
    // LookupPointer reg, data_index
    code.emplace_back(static_cast<uint8_t>(EOpcode::LookupPointer), 
                     result_reg, 0, 0, static_cast<int32_t>(data_index));
    
    return result_reg;
}

size_t Compiler::compile_number(const ASTNode* node, size_t result_reg) {
    std::string num_str(node->token.text);
    int value = std::stoi(num_str);
    
    // LoadImediateInt reg, value
    code.emplace_back(static_cast<uint8_t>(EOpcode::LoadImediateInt),
                     result_reg, 0, 0, value);
    
    return result_reg;
}

size_t Compiler::compile_string(const ASTNode* node, size_t result_reg) {
    std::string str(node->token.text);
    size_t data_index = add_string_constant(str);
    
    // LookupPointer reg, data_index  
    code.emplace_back(static_cast<uint8_t>(EOpcode::LookupPointer),
                     result_reg, 0, 0, static_cast<int32_t>(data_index));
    
    return result_reg;
}

size_t Compiler::compile_call(const ASTNode* node, size_t result_reg) {
    if (node->children.size() < 1) return result_reg;
    
    // 1. Компилируем функцию
    size_t func_reg = allocate_register();
    compile_expression(node->children[0].get(), func_reg);
    
    // 2. Компилируем аргументы в регистры R24, R25, R26...
    for (size_t i = 1; i < node->children.size(); i++) {
        size_t arg_reg = ARGUMENT_REGISTERS_OFFSET + (i - 1);
        compile_expression(node->children[i].get(), arg_reg);
        // НЕ вызываем Call для каждого аргумента!
    }
    
    // 3. ОДИН вызов функции
    code.emplace_back(static_cast<uint8_t>(EOpcode::Call),
                     static_cast<uint8_t>(func_reg), 
                     static_cast<uint8_t>(result_reg), 
                     static_cast<uint8_t>(node->children.size() - 1));
    
    return result_reg;
}

// ДОБАВЛЯЕМ ПРОПУЩЕННЫЙ МЕТОД:
size_t Compiler::compile_list_data(const ASTNode* node, size_t result_reg) {
    // Пока просто компилируем первый элемент
    // TODO: полная реализация для списков
    if (node->children.size() > 0) {
        return compile_expression(node->children[0].get(), result_reg);
    }
    return result_reg;
}

size_t Compiler::compile_list(const ASTNode* node, size_t result_reg) {
    // Проверяем, является ли это вызовом функции
    if (node->children.size() > 0 && 
        node->children[0]->type == ObjectType::SYMBOL) {
        return compile_call(node, result_reg);
    } else {
        // Иначе это данные - компилируем как список
        return compile_list_data(node, result_reg);
    }
}

size_t Compiler::compile_expression(const ASTNode* node, size_t result_reg) {
    if (!node) return result_reg;
    
    switch(node->type) {
        case ObjectType::SYMBOL:
            return compile_symbol(node, result_reg);
        case ObjectType::INTEGER:
            return compile_number(node, result_reg);
        case ObjectType::STRING:
            return compile_string(node, result_reg);
        case ObjectType::PAIR:
            return compile_list(node, result_reg);
        default:
            return result_reg;
    }
}

// ДОБАВЛЯЕМ ПРОПУЩЕННЫЙ МЕТОД:
size_t Compiler::allocate_register() {
    if (next_register >= DC_FRAME_MAX_REGISTERS_NUM) {
        compile_error(nullptr, "Too many registers allocated");
    }
    return next_register++;
}

void Compiler::compile_error(const ASTNode* node, const std::string& message) {
    SourceInfo info = g_source_db.get_info(node);
    if (info.filename.empty()) {
        info = g_source_db.get_info(&node->token);
    }
    
    if (info.filename.empty()) {
        std::cerr << "Error: " << message << std::endl;
    } else {
        // Используем format_error из SourceInfo
        std::cerr << info.format_error(message) << std::endl;
    }
    throw std::runtime_error("Compilation error");
}

std::vector<Instruction> Compiler::compile(const ASTNode* ast) {
    code.clear();
    data.clear();
    next_register = 0;
    
    if (ast->type == ObjectType::PROGRAM) {
        for (const auto& expr : ast->children) {
            size_t result_reg = allocate_register();
            compile_expression(expr.get(), result_reg);
        }
    } else {
        size_t result_reg = allocate_register();
        compile_expression(ast, result_reg);
    }
    
    // Добавляем Return в конец
    code.emplace_back(static_cast<uint8_t>(EOpcode::Return), 0);
    
    return code;
}

void Compiler::print_bytecode() const {
    std::cout << "=== BYTECODE ===" << std::endl;
    
    // УДАЛЯЕМ хардкод тестовых строк - теперь показываем реальные данные
    std::cout << "Data section: " << data.size() << " entries" << std::endl;
    for (size_t i = 0; i < data.size(); i++) {
        uint32_t crc_value = static_cast<uint32_t>(data[i]);
        std::cout << "  data[" << i << "] = " << data[i] 
                  << " (0x" << std::hex << crc_value << std::dec << ")";
        
        // Просто показываем CRC32, без попыток угадать строку
        // Позже подключим StringDB для настоящего reverse lookup
        if (data[i] > -1000 && data[i] < 1000) {
            std::cout << " [number: " << data[i] << "]";
        } else {
            std::cout << " [crc32]";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\nCode section: " << code.size() << " instructions" << std::endl;
    for (size_t i = 0; i < code.size(); i++) {
        const auto& instr = code[i];
        std::cout << "  " << std::dec << i << ": ";
        
        switch(static_cast<EOpcode>(instr.opcode)) {
            case EOpcode::LookupPointer: 
                std::cout << "LookupPointer R" << static_cast<int>(instr.a) 
                          << ", data[" << instr.k << "]";
                break;
                
            case EOpcode::LoadImediateInt: 
                std::cout << "LoadImediateInt R" << static_cast<int>(instr.a) 
                          << ", " << instr.k;
                break;
                
            case EOpcode::Call: 
                std::cout << "Call R" << static_cast<int>(instr.a) 
                          << ", R" << static_cast<int>(instr.b) 
                          << ", " << static_cast<int>(instr.c) << " args";
                break;
                
            case EOpcode::Return: 
                std::cout << "Return";
                break;
                
            default: 
                std::cout << "Op" << static_cast<int>(instr.opcode)
                          << " R" << static_cast<int>(instr.a)
                          << ", R" << static_cast<int>(instr.b)
                          << ", R" << static_cast<int>(instr.c)
                          << ", " << instr.k;
        }
        std::cout << std::endl;
    }
}