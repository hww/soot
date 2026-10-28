#include "sootc/node/NodeInspector.hpp"

#include "sootc/node/AtNode.hpp"
#include "sootc/node/BinaryNode.hpp"
#include "sootc/node/CallNode.hpp"
#include "sootc/node/CastNode.hpp"
#include "sootc/node/CompareNode.hpp"
#include "sootc/node/ConstNode.hpp"
#include "sootc/node/DataDeclarationNode.hpp"
#include "sootc/node/DerefNode.hpp"
#include "sootc/node/EnumDeclarationNode.hpp"
#include "sootc/node/FileNode.hpp"
#include "sootc/node/FunctionNode.hpp"
#include "sootc/node/GlobalNode.hpp"
#include "sootc/node/HeapDefineNode.hpp"
#include "sootc/node/IfNode.hpp"
#include "sootc/node/LetNode.hpp"
#include "sootc/node/MethodCallNode.hpp"
#include "sootc/node/NewNode.hpp"
#include "sootc/node/ReturnNode.hpp"
#include "sootc/node/SequenceNode.hpp"
#include "sootc/node/SetNode.hpp"
#include "sootc/node/StateScriptNode.hpp"
#include "sootc/node/StaticDefineNode.hpp"
#include "sootc/node/StoreFieldNode.hpp"
#include "sootc/node/StoreGlobalNode.hpp"
#include "sootc/node/TypeDeclarationNode.hpp"
#include "sootc/node/UnaryNode.hpp"
#include "sootc/node/VariableNode.hpp"
#include "sootc/node/WhileNode.hpp"

#include "type_system/Type.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace sootc {

    namespace {

        std::string indent_str(int n) { return std::string(static_cast<size_t>(n), ' '); }

        std::string type_str(const Type *t) { return t ? "\"" + t->get_name() + "\"" : "(null)"; }

        std::string bool_str(bool b) { return b ? "#t" : "#f"; }

        std::string esc(const std::string &s) {
            std::string out;
            out.reserve(s.size() + 2);
            for (char c : s) {
                switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
                }
            }
            return out;
        }

        std::string str(const std::string &s) { return "\"" + esc(s) + "\""; }

        // Forward.
        void print_node(const Node *n, std::ostream &os, int indent);

        void print_children(const Node *n, std::ostream &os, int indent) {
            for (const auto &c : n->all_children()) {
                os << '\n';
                print_node(c, os, indent + 2);
            }
        }

        // Print a header line: (TypeName field=value ...)
        struct Printer {
            std::ostream &os;
            int           indent;

            void open(const char *type_name) { os << indent_str(indent) << '(' << type_name; }

            void field(const char *key, const std::string &val) { os << ' ' << key << '=' << val; }

            void close_with_children(const Node *n) {
                if (!n->all_children().empty()) {
                    print_children(n, os, indent);
                    os << '\n' << indent_str(indent) << ')';
                } else {
                    os << ')';
                }
            }
        };

        void print_node(const Node *n, std::ostream &os, int indent) {
            if (!n) {
                os << indent_str(indent) << "(null)";
                return;
            }

            Printer p{os, indent};

            if (auto *x = dynamic_cast<const FileNode *>(n)) {
                p.open("FileNode");
                p.field("name", str(x->name()));
                p.field("imports", std::to_string(x->imports().size()));
                p.field("requires", std::to_string(x->get_requires().size()));
                p.field("children", std::to_string(x->children().size()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const FunctionNode *>(n)) {
                p.open("FunctionNode");
                p.field("name", str(x->name()));
                p.field("params", std::to_string(x->param_count()));
                p.field("return", type_str(x->get_return_type()));
                p.field("method_of", str(x->method_of_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const SequenceNode *>(n)) {
                p.open("SequenceNode");
                p.field("size", std::to_string(x->size()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const VariableNode *>(n)) {
                p.open("VariableNode");
                p.field("name", str(x->name()));
                p.field("type", type_str(x->get_type()));
                p.field("elem_type", type_str(x->elem_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const ConstNode *>(n)) {
                p.open("ConstNode");
                if (x->is_int()) p.field("int", std::to_string(x->int_value()));
                else if (x->is_float())
                    p.field("float", std::to_string(x->float_value()));
                else if (x->is_string())
                    p.field("string", str(x->string_value()));
                else
                    p.field("kind", "<undefined>");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const BinaryNode *>(n)) {
                p.open("BinaryNode");
                const char *op = "?";
                switch (x->op()) {
                case BinaryNode::Op::ADD: op = "+"; break;
                case BinaryNode::Op::SUB: op = "-"; break;
                case BinaryNode::Op::MUL: op = "*"; break;
                case BinaryNode::Op::DIV: op = "/"; break;
                case BinaryNode::Op::MOD: op = "%"; break;
                }
                p.field("op", str(op));
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const CompareNode *>(n)) {
                p.open("CompareNode");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const CallNode *>(n)) {
                p.open("CallNode");
                p.field("fn", str(x->to_string()));
                p.field("is_native", bool_str(x->is_native()));
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const MethodCallNode *>(n)) {
                p.open("MethodCallNode");
                p.field("is_native", bool_str(x->is_native()));
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const DerefNode *>(n)) {
                p.open("DerefNode");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const AtNode *>(n)) {
                p.open("AtNode");
                p.field("elem_type", type_str(x->elem_type()));
                p.close_with_children(n);
                return;
            }

            if (dynamic_cast<const IfNode *>(n)) {
                p.open("IfNode");
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const WhileNode *>(n)) {
                p.open("WhileNode");
                p.field("label", x->label() ? str(*x->label()) : "(none)");
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const LetNode *>(n)) {
                p.open("LetNode");
                p.field("bindings", std::to_string(x->bindings().size()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const SetNode *>(n)) {
                p.open("SetNode");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const ReturnNode *>(n)) {
                p.open("ReturnNode");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const NewNode *>(n)) {
                p.open("NewNode");
                p.field("alloc", str(x->allocation()));
                p.field("type", type_str(x->get_type()));
                p.field("inline_array", bool_str(x->is_inline_array()));
                if (x->is_inline_array()) {
                    p.field("elem", str(x->elem_type_name()));
                    p.field("count", std::to_string(x->array_size()));
                }
                p.field("fields", std::to_string(x->fields().size()));
                p.field("args", std::to_string(x->arguments().size()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const DataDeclarationNode *>(n)) {
                p.open("DataDeclarationNode");
                p.field("name", str(x->name()));
                p.field("heap", bool_str(x->is_heap()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const StoreGlobalNode *>(n)) {
                p.open("StoreGlobalNode");
                p.field("name", str(x->name()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const StoreFieldNode *>(n)) {
                p.open("StoreFieldNode");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (dynamic_cast<const HeapDefineNode *>(n)) {
                p.open("HeapDefineNode");
                p.close_with_children(n);
                return;
            }

            if (dynamic_cast<const StaticDefineNode *>(n)) {
                p.open("StaticDefineNode");
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const TypeDeclarationNode *>(n)) {
                p.open("TypeDeclarationNode");
                p.field("type", str(x->type().print()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const StateScriptNode *>(n)) {
                p.open("StateScriptNode");
                p.field("type", str(x->type().print()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const EnumDeclarationNode *>(n)) {
                p.open("EnumDeclarationNode");
                p.field("name", str(x->name()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const GlobalNode *>(n)) {
                p.open("GlobalNode");
                p.field("symbols", std::to_string(x->symbols().size()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const CastNode *>(n)) {
                p.open("CastNode");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            if (auto *x = dynamic_cast<const UnaryNode *>(n)) {
                p.open("UnaryNode");
                p.field("type", type_str(x->get_type()));
                p.close_with_children(n);
                return;
            }

            // Fallback: unknown node type — print node_type() and children.
            p.open(n->node_type());
            p.close_with_children(n);
        }

    } // namespace

    void inspect_ast(const Node *root, std::ostream &os, int indent) {
        print_node(root, os, indent);
        os << '\n';
    }

    void inspect_ast(const Node *root) { inspect_ast(root, std::cout, 0); }

} // namespace sootc