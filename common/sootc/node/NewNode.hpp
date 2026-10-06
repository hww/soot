#pragma once

#include "ExpressionNode.hpp"
#include "common/type_system/TypeSpec.hpp"
#include "sootc/node/FunctionNode.hpp"             // ← ДОЛЖНО БЫТЬ
#include "sootc/node/Node.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sootc {

    /// @brief (new Type :field value ...)
    /// @details Creates an instance of a struct type. Fields not explicitly
    ///          given are initialized to their default values (0 / #f).
    ///
    /// @note Currently the node only parses and holds the AST. Serialization
    ///       into a data-instance entry is not implemented yet — the `generate`
    ///       method returns an empty ProgramBinaryElement.
    class NewNode : public ExpressionNode {
    public:
        struct FieldInit {
            std::string                     name;
            std::unique_ptr<ExpressionNode> value;
        };

    private:
        TypeSpec               m_type;
        std::vector<FieldInit> m_fields;

    public:
        explicit NewNode(TypeSpec type)
            : ExpressionNode(NodeType::NewNode), m_type(std::move(type)) {}

        void add_field(const std::string &name, std::unique_ptr<ExpressionNode> value) {
            m_fields.push_back({name, std::move(value)});
        }

        const TypeSpec               &type() const { return m_type; }
        const std::vector<FieldInit> &fields() const { return m_fields; }

        void emit(FunctionNode &fn) override {
            // In a function context, `new` should allocate and initialize.
            // For now we don't generate code for `new` — we only need to make
            // sure that a temp register is registered for this node, otherwise
            // SequenceNode / ReturnNode will fail to look it up.
            //
            // We compute all field values (in case they have side effects),
            // then declare the result as temp_reg 0 (the return register).

            for (auto &f : m_fields) {
                if (f.value) { f.value->emit(fn); }
            }

            // The result of `new` is a pointer. For now use register 0 as a
            // placeholder, so that downstream nodes don't crash on lookup.
            fn.set_temp_reg(this, 0);
        }

        std::string to_string() const override {
            std::string result = "(new " + m_type.print();
            for (const auto &f : m_fields) {
                result += " :" + f.name + " " + (f.value ? f.value->to_string() : "none");
            }
            result += ")";
            return result;
        }

        ProgramBinaryElement generate(GlobalState &state) override {
            (void)state;
            // TODO: serialize into a data-instance entry.
            return ProgramBinaryElement(0);
        }
    };

} // namespace sootc