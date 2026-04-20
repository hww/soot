#include "common/sootc/compiler/FileCompiler.hpp"
#include "common/sootc/compiler/NodeBuilder.hpp"
#include "common/sootc/node/FileNode.hpp"
#include "common/sootc/node/GlobalNode.hpp"

namespace sootc {
namespace FileCompiler {

std::unique_ptr<FileNode> compile(const script::Object& forms, 
                                   const std::string& filename, 
                                   Compiler& compiler) {
    NodeBuilder builder(compiler.ts(), &compiler);
    
    auto file_node = std::make_unique<FileNode>(filename);
    
    auto current = forms;
    while (current.is_pair()) {
        auto node = builder.build(current.as_pair()->car, file_node.get());
        if (node) {
            file_node->add_child(std::move(node));
        }
        current = current.as_pair()->cdr;
    }
    
    return file_node;
}

} // namespace FileCompiler
} // namespace sootc