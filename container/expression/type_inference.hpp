#pragma once

#include "ast.hpp"
#include <stdexcept>
#include <unordered_map>

namespace franklin::expr {

class TypeInference : public ExprVisitor {
public:
  // Main entry point: infer types for entire AST
  void infer(ExprNode* node) {
    if (!node)
      return;
    node->accept(*this);
  }

  // Visitor implementations
  void visit(ColumnRefNode& node) override {
    // Type is already set during parsing/column registration
    // Nothing to do here
  }

  void visit(ConstantNode& node) override {
    // Type is already set during construction
    // Nothing to do here
  }

  void visit(UnaryOpNode& node) override {
    // Infer child type first
    if (node.child) {
      node.child->accept(*this);

      // Result type depends on operation
      if (node.op == OpType::BitNot) {
        if (is_floating_type(node.child->result_type)) {
          throw std::runtime_error("Bitwise NOT requires integral type");
        }
        node.result_type = node.child->result_type;
      } else if (node.op == OpType::LogicalNot) {
        node.result_type = DataType::Bool;
      } else {
        node.result_type = node.child->result_type;
      }
    }
  }

  void visit(BinaryOpNode& node) override {
    // Infer children types first (bottom-up)
    if (node.left)
      node.left->accept(*this);
    if (node.right)
      node.right->accept(*this);

    if (!node.left || !node.right) {
      node.result_type = DataType::Unknown;
      return;
    }

    DataType lhs = node.left->result_type;
    DataType rhs = node.right->result_type;

    // Apply standard C++ type promotion rules
    node.result_type = promote(lhs, rhs, node.op);
  }

  void visit(TernaryNode& node) override {
    // Infer all branches
    if (node.condition)
      node.condition->accept(*this);
    if (node.true_branch)
      node.true_branch->accept(*this);
    if (node.false_branch)
      node.false_branch->accept(*this);

    if (!node.true_branch || !node.false_branch) {
      node.result_type = DataType::Unknown;
      return;
    }

    // Condition must be boolean or comparable to zero
    if (node.condition && !is_comparable_type(node.condition->result_type)) {
      throw std::runtime_error(
          "Ternary condition must be boolean or comparable");
    }

    // Result type is promoted from true/false branches
    node.result_type =
        promote(node.true_branch->result_type, node.false_branch->result_type,
                OpType::Add); // Use Add for general promotion
  }

  void visit(CastNode& node) override {
    // Infer child type first
    if (node.child) {
      node.child->accept(*this);
    }

    // Result type is the target type (already set)
    node.result_type = node.target_type;
  }

private:
  DataType promote(DataType lhs, DataType rhs, OpType op) {
    // Bitwise operations require integral types
    if (is_bitwise_op(op)) {
      if (is_floating_type(lhs) || is_floating_type(rhs)) {
        throw std::runtime_error("Bitwise operations require integral types");
      }
      return wider_int(lhs, rhs);
    }

    // Comparison operations return bool
    if (is_comparison_op(op)) {
      return DataType::Bool;
    }

    // Logical operations require bool and return bool
    if (is_logical_op(op)) {
      return DataType::Bool;
    }

    // Arithmetic: float beats int, wider beats narrower
    if (is_floating_type(lhs) || is_floating_type(rhs)) {
      return wider_of(to_float(lhs), to_float(rhs));
    }

    return wider_int(lhs, rhs);
  }

  DataType wider_of(DataType a, DataType b) {
    static const std::unordered_map<DataType, int> rank = {
        {DataType::Bool, 0},    {DataType::Int8, 1},   {DataType::UInt8, 2},
        {DataType::Int16, 3},   {DataType::UInt16, 4}, {DataType::Int32, 5},
        {DataType::UInt32, 6},  {DataType::Int64, 7},  {DataType::UInt64, 8},
        {DataType::Float16, 9}, {DataType::BF16, 10},  {DataType::Float32, 11},
        {DataType::Float64, 12}};

    auto it_a = rank.find(a);
    auto it_b = rank.find(b);

    if (it_a == rank.end() || it_b == rank.end()) {
      return DataType::Unknown;
    }

    return it_a->second > it_b->second ? a : b;
  }

  DataType wider_int(DataType a, DataType b) {
    if (!is_integral_type(a) || !is_integral_type(b)) {
      return DataType::Unknown;
    }

    // If one is unsigned and one is signed, promote carefully
    bool a_signed = is_signed_type(a);
    bool b_signed = is_signed_type(b);

    if (a_signed != b_signed) {
      // Mixed signedness: promote to larger signed or unsigned
      size_t a_size = sizeof_type(a);
      size_t b_size = sizeof_type(b);

      if (a_size > b_size)
        return a;
      if (b_size > a_size)
        return b;

      // Same size: prefer signed for safety
      return a_signed ? a : b;
    }

    // Both same signedness: pick wider
    return wider_of(a, b);
  }

  DataType to_float(DataType type) {
    if (is_floating_type(type)) {
      return type;
    }

    // Promote integral types to appropriate float
    size_t size = sizeof_type(type);
    if (size <= 2)
      return DataType::Float32;
    if (size == 4)
      return DataType::Float32;
    return DataType::Float64;
  }

  bool is_comparable_type(DataType type) {
    return type == DataType::Bool || is_integral_type(type) ||
           is_floating_type(type);
  }
};

} // namespace franklin::expr
