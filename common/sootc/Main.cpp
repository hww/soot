// common/sootc/Main.cpp
#include "Compiler/FunctionCompiler.hpp"
#include "IR/IR_Value.hpp"
#include "IR/IR_Node.hpp"
#include "common/type_system/TypeSystem.hpp"
#include "common/util/Log.hpp"
#include "common/carbon/files/DciFile.hpp"  // ДОБАВИТЬ
#include <fstream>

using namespace sootc;

int main() {
    lg::info("=== SOOT Compiler Test ===");
    
    TypeSystem& ts = TypeSystem::instance();
    ts.add_builtin_types();
    
    Type* function_type = ts.lookup_type_no_throw("function");
    if (!function_type) {
        lg::error("Type 'function' not found");
        return 1;
    }
    
    Type* int_type = ts.lookup_type_no_throw("int");
    if (!int_type) {
        lg::error("Type 'int' not found");
        return 1;
    }
    
    // Создаем компилятор
    FunctionCompiler compiler(ts, function_type);
    
    // Создаем регистры
    IR_Reg* a = compiler.create_local_reg(int_type);
    IR_Reg* b = compiler.create_local_reg(int_type);
    IR_Reg* result = compiler.create_local_reg(int_type);
    
    // result = a + b
    compiler.add_node(std::make_unique<IR_Binary>(
        IR_Binary::Op::ADD, result, a, b));
    
    // return result
    compiler.add_node(std::make_unique<IR_Return>(result));
    
    // Выводим IR
    lg::info("IR:");
    lg::info("{}", compiler.to_string());
    
    // Компилируем в байткод
    std::vector<u8> bytecode = compiler.compile();
    
    lg::info("Bytecode size: {} bytes", bytecode.size());
    
    if (!bytecode.empty()) {
        std::string hex;
        for (size_t i = 0; i < std::min(bytecode.size(), size_t(64)); i++) {
            hex += fmt::format("{:02x} ", bytecode[i]);
        }
        lg::info("First bytes: {}", hex);
    }
    
    // ============================================================
    // СОХРАНЯЕМ В ФАЙЛЫ
    // ============================================================
    
    std::string module_name = "add";
    std::string logical_path = "math/" + module_name;
    
    // 1. Сохраняем .bin файл
    std::string bin_filename = module_name + ".bin";
    std::ofstream bin_file(bin_filename, std::ios::binary);
    if (bin_file.is_open()) {
        bin_file.write(reinterpret_cast<const char*>(bytecode.data()), bytecode.size());
        bin_file.close();
        lg::info("Saved bytecode to: {}", bin_filename);
        lg::info("  Size: {} bytes", bytecode.size());
    } else {
        lg::error("Failed to create .bin file: {}", bin_filename);
        return 1;
    }
    
    // 2. Сохраняем .dci файл
    runtime::files::DciFile dci;
    dci.logical_path = logical_path;
    dci.module_name = module_name;
    dci.binary_size = static_cast<u32>(bytecode.size());
    dci.exports.push_back(SID("add"));
    
    std::string dci_filename = module_name + ".dci";
    if (dci.save(dci_filename)) {
        lg::info("Saved DCI to: {}", dci_filename);
    } else {
        lg::error("Failed to create .dci file: {}", dci_filename);
        return 1;
    }
    
    lg::info("=== Compilation complete ===");
    
    return 0;
}