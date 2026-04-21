#pragma once

#include "common/sooti/Object.hpp"
#include <memory>
#include <string>

namespace sootc {

class FileNode;
class Compiler;

namespace FileCompiler {

// Только строит дерево из AST
std::unique_ptr<FileNode> compile(const script::Object& forms, 
                                   const std::string& filename, 
                                   Compiler& compiler);

} // namespace FileCompiler

} // namespace sootc