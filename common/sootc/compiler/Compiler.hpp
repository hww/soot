// Compiler.hpp
#pragma once

#include "common/type_system/TypeSystem.hpp"
#include "common/sooti/Object.hpp"
#include "common/carbon/file/BinaryFile.hpp"
#include <string>
#include <filesystem>
#include <memory>
#include <expected>

namespace sootc {

class Node;
class GlobalNode;

class Compiler {
public:
    explicit Compiler(TypeSystem& ts);

    /**
     * @brief Главный метод: путь к файлу -> готовый бинарник DC00
     */
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(const std::filesystem::path& path);
    
    /**
     * @brief Главный метод: путь к файлу -> готовый бинарник DC00
     */
    [[nodiscard]] std::expected<std::unique_ptr<BinaryFile>, std::string> 
    compile_file(script::Object& forms, const std::string& filename);

    // Доступ к системе типов
    TypeSystem& ts() { return ts_; }
    
private:
    TypeSystem& ts_;
};

} // namespace sootc