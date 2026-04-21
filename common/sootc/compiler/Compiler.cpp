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
    script::Reader reader;
    auto forms = reader.read_from_file({ path.string() }, true, false);
    if (forms.is_null()) {
        return std::unexpected("Failed to read or parse file: " + path.string());
    }
    return compile_file(forms, path.string());
}

std::expected<std::unique_ptr<BinaryFile>, std::string> 
Compiler::compile_file(script::Object& forms, const std::string& filename) {
    NodeBuilder builder(ts_, this);
    
    auto file_node = std::make_unique<FileNode>(filename);
    
    auto current = forms;
    while (current.is_pair()) {
        auto node = builder.build(current.as_pair()->car, file_node.get());
        if (node) {
            file_node->add_child(std::move(node));
        }
        current = current.as_pair()->cdr;
    }
    
    GlobalState state;
    auto element = file_node->generate(state);
    
    // Используем свободную функцию
    auto bytes = make_aligned_buffer(element.m_rawData.size());
    std::memcpy(bytes.get(), element.m_rawData.data(), element.m_rawData.size());
    
    return BinaryFile::from_buffer(filename, std::move(bytes), element.m_rawData.size())
        .transform([](BinaryFile&& file) {
            return std::make_unique<BinaryFile>(std::move(file));
        });
}
} // namespace sootc