// Compiler.cpp
#include "sootc/compiler/Compiler.hpp"
#include "sootc/compiler/FileCompiler.hpp"
#include "sootc/compiler/NodeBuilder.hpp"
#include "sootc/node/GlobalNode.hpp"
#include "sootc/node/FileNode.hpp"
#include "common/sooti/Reader.hpp"
#include "common/util/Log.hpp"


namespace sootc {

Compiler::Compiler(TypeSystem& ts) : ts_(ts) {}

std::expected<std::unique_ptr<BinaryFile>, std::string> 
Compiler::compile_file(const std::filesystem::path& path) {
    // 1. Читаем файл
    script::Reader reader;
    auto forms = reader.read_from_file({ path.string() }, true, false);
    if (forms.is_null()) {
        return std::unexpected("Failed to read or parse file: " + path.string());
    }

    return compile_file(forms, path.string());
}

std::expected<std::unique_ptr<BinaryFile>, std::string> 
Compiler::compile_file(script::Object& forms, const std::string& filename) {
    lg::info("Compiling file: {}", filename);
    
    NodeBuilder builder(ts_, this);
    
    auto global = std::make_unique<GlobalNode>();
    auto file_node = std::make_unique<FileNode>(filename);
    
    auto current = forms;
    while (current.is_pair()) {
        auto node = builder.build(current.as_pair()->car, file_node.get());
        if (node) {
            file_node->add_child(std::move(node));
        }
        current = current.as_pair()->cdr;
    }
    
    global->add_child(std::move(file_node));
    
    // Собираем ProgramBinaryElement для всех функций
    std::vector<ProgramBinaryElement> functions;
    
    for (auto& child : global->children()) {
        if (auto* file = dynamic_cast<FileNode*>(child.get())) {
            for (auto& fn_child : file->children()) {
                if (auto* fn = dynamic_cast<FunctionNode*>(fn_child.get())) {
                    fn->emit_body();
                    functions.push_back(fn->build_binary(fn->name()));
                    lg::info("Function '{}': {} instructions", 
                             fn->name(), fn->instructions().size());
                }
            }
        }
    }
    
    if (functions.empty()) {
        return std::unexpected("No functions found in file");
    }
    
    // Собираем финальный бинарник (статический вызов)
    GlobalState state;
    auto binary_result = FileCompiler::make_binary(std::move(functions), state);
    
    if (!binary_result) {
        return std::unexpected(binary_result.error());
    }
    
    auto& [bytes, size] = *binary_result;
    
    return BinaryFile::from_buffer(filename, std::move(bytes), size)
        .transform([](BinaryFile&& file) {
            return std::make_unique<BinaryFile>(std::move(file));
        });
}

} // namespace sootc