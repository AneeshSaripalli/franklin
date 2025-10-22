#include "ast.hpp"

namespace franklin::expr {

// ColumnRefNode implementations
void ColumnRefNode::accept(ExprVisitor& v) {
  v.visit(*this);
}

std::unique_ptr<ExprNode> ColumnRefNode::clone() const {
  auto node = std::make_unique<ColumnRefNode>(name, column_index);
  node->result_type = result_type;
  node->complexity = complexity;
  node->memory_loads = memory_loads;
  node->is_constant = is_constant;
  node->can_fuse = can_fuse;
  node->register_pressure = register_pressure;
  return node;
}

// ConstantNode implementations
void ConstantNode::accept(ExprVisitor& v) {
  v.visit(*this);
}

std::unique_ptr<ExprNode> ConstantNode::clone() const {
  auto node = std::make_unique<ConstantNode>();
  node->value = value;
  node->result_type = result_type;
  node->complexity = complexity;
  node->memory_loads = memory_loads;
  node->is_constant = is_constant;
  node->can_fuse = can_fuse;
  node->register_pressure = register_pressure;
  return node;
}

// UnaryOpNode implementations
void UnaryOpNode::accept(ExprVisitor& v) {
  v.visit(*this);
}

std::unique_ptr<ExprNode> UnaryOpNode::clone() const {
  auto node = std::make_unique<UnaryOpNode>();
  node->op = op;
  if (child) {
    node->child = child->clone();
  }
  node->result_type = result_type;
  node->complexity = complexity;
  node->memory_loads = memory_loads;
  node->is_constant = is_constant;
  node->can_fuse = can_fuse;
  node->register_pressure = register_pressure;
  return node;
}

// BinaryOpNode implementations
void BinaryOpNode::accept(ExprVisitor& v) {
  v.visit(*this);
}

std::unique_ptr<ExprNode> BinaryOpNode::clone() const {
  auto node = std::make_unique<BinaryOpNode>();
  node->op = op;
  if (left) {
    node->left = left->clone();
  }
  if (right) {
    node->right = right->clone();
  }
  node->result_type = result_type;
  node->complexity = complexity;
  node->memory_loads = memory_loads;
  node->is_constant = is_constant;
  node->can_fuse = can_fuse;
  node->register_pressure = register_pressure;
  node->fusion_root = fusion_root;
  node->fusion_group = fusion_group;
  return node;
}

// TernaryNode implementations
void TernaryNode::accept(ExprVisitor& v) {
  v.visit(*this);
}

std::unique_ptr<ExprNode> TernaryNode::clone() const {
  auto node = std::make_unique<TernaryNode>();
  if (condition) {
    node->condition = condition->clone();
  }
  if (true_branch) {
    node->true_branch = true_branch->clone();
  }
  if (false_branch) {
    node->false_branch = false_branch->clone();
  }
  node->result_type = result_type;
  node->complexity = complexity;
  node->memory_loads = memory_loads;
  node->is_constant = is_constant;
  node->can_fuse = can_fuse;
  node->register_pressure = register_pressure;
  return node;
}

// CastNode implementations
void CastNode::accept(ExprVisitor& v) {
  v.visit(*this);
}

std::unique_ptr<ExprNode> CastNode::clone() const {
  auto node = std::make_unique<CastNode>();
  node->target_type = target_type;
  if (child) {
    node->child = child->clone();
  }
  node->result_type = result_type;
  node->complexity = complexity;
  node->memory_loads = memory_loads;
  node->is_constant = is_constant;
  node->can_fuse = can_fuse;
  node->register_pressure = register_pressure;
  return node;
}

} // namespace franklin::expr
