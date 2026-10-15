#pragma once
#include "BinaryNode.hpp"
#include "ConstNode.hpp"
#include "UnaryNode.hpp"

namespace sootc {

    // Try to fold an expression into a ConstNode at compile time.
    // Returns nullptr if the expression is not a compile-time constant.
    // The caller owns the returned unique_ptr.
    inline std::unique_ptr<ConstNode> try_fold_const(const ExpressionNode *expr) {
        if (!expr) return nullptr;

        // Already a constant?
        if (auto *cn = dynamic_cast<const ConstNode *>(expr)) {
            // Return a clone. ConstNode has no clone, so rebuild from value.
            if (cn->is_int()) return ConstNode::make_int(cn->int_value());
            if (cn->is_float()) return ConstNode::make_float(cn->float_value());
            if (cn->is_string()) return ConstNode::make_string(cn->string_value());
            return nullptr;
        }

        // Binary arithmetic?
        if (auto *bn = dynamic_cast<const BinaryNode *>(expr)) {
            auto left = try_fold_const(bn->left());
            auto right = try_fold_const(bn->right());
            if (!left || !right) return nullptr;

            if (left->is_int() && right->is_int()) {
                const i64 a = left->int_value();
                const i64 b = right->int_value();
                switch (bn->op()) {
                case BinaryNode::Op::ADD: return ConstNode::make_int(a + b);
                case BinaryNode::Op::SUB: return ConstNode::make_int(a - b);
                case BinaryNode::Op::MUL: return ConstNode::make_int(a * b);
                case BinaryNode::Op::DIV:
                    if (b == 0) return nullptr;
                    return ConstNode::make_int(a / b);
                case BinaryNode::Op::MOD:
                    if (b == 0) return nullptr;
                    return ConstNode::make_int(a % b);
                }
            }
            if (left->is_float() && right->is_float()) {
                const f64 a = left->float_value();
                const f64 b = right->float_value();
                switch (bn->op()) {
                case BinaryNode::Op::ADD: return ConstNode::make_float(a + b);
                case BinaryNode::Op::SUB: return ConstNode::make_float(a - b);
                case BinaryNode::Op::MUL: return ConstNode::make_float(a * b);
                case BinaryNode::Op::DIV:
                    if (b == 0.0) return nullptr;
                    return ConstNode::make_float(a / b);
                case BinaryNode::Op::MOD: return nullptr;
                }
            }
        }

        return nullptr;
    }

} // namespace sootc