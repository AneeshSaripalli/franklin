#pragma once

#include "ast.hpp"
#include <cmath>
#include <functional>
#include <unordered_map>

namespace franklin::expr {

class ExpressionOptimizer {
public:
  void optimize(std::unique_ptr<ExprNode>& root) {
    if (!root)
      return;

    // Pass 1: Constant folding
    constant_folding(root);

    // Pass 2: Algebraic simplification
    algebraic_simplification(root);

    // Pass 3: Strength reduction
    strength_reduction(root);

    // Pass 4: Cast elimination
    fold_casts(root);

    // Pass 5: Conversion hoisting (done after casts are folded)
    hoist_conversions(root);
  }

private:
  // Pass 1: Fold constant expressions at compile time
  void constant_folding(std::unique_ptr<ExprNode>& node) {
    if (auto* binop = dynamic_cast<BinaryOpNode*>(node.get())) {
      constant_folding(binop->left);
      constant_folding(binop->right);

      // If both children are constants, evaluate now
      auto* left_const = dynamic_cast<ConstantNode*>(binop->left.get());
      auto* right_const = dynamic_cast<ConstantNode*>(binop->right.get());

      if (left_const && right_const) {
        auto result = evaluate_constant(binop->op, left_const, right_const);
        if (result) {
          node = std::move(result);
        }
      }
    } else if (auto* unary = dynamic_cast<UnaryOpNode*>(node.get())) {
      constant_folding(unary->child);

      if (auto* child_const = dynamic_cast<ConstantNode*>(unary->child.get())) {
        auto result = evaluate_unary_constant(unary->op, child_const);
        if (result) {
          node = std::move(result);
        }
      }
    } else if (auto* cast = dynamic_cast<CastNode*>(node.get())) {
      constant_folding(cast->child);
    } else if (auto* ternary = dynamic_cast<TernaryNode*>(node.get())) {
      constant_folding(ternary->condition);
      constant_folding(ternary->true_branch);
      constant_folding(ternary->false_branch);

      // If condition is constant, select branch at compile time
      if (auto* cond_const =
              dynamic_cast<ConstantNode*>(ternary->condition.get())) {
        bool cond = cond_const->value.i64 != 0;
        node = std::move(cond ? ternary->true_branch : ternary->false_branch);
      }
    }
  }

  // Pass 2: Algebraic identities
  void algebraic_simplification(std::unique_ptr<ExprNode>& node) {
    if (auto* binop = dynamic_cast<BinaryOpNode*>(node.get())) {
      algebraic_simplification(binop->left);
      algebraic_simplification(binop->right);

      // x + 0 → x
      if (binop->op == OpType::Add && is_zero(binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // 0 + x → x
      if (binop->op == OpType::Add && is_zero(binop->left.get())) {
        node = std::move(binop->right);
        return;
      }

      // x - 0 → x
      if (binop->op == OpType::Sub && is_zero(binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // x * 0 → 0
      if (binop->op == OpType::Mul && is_zero(binop->right.get())) {
        node = std::make_unique<ConstantNode>(static_cast<int64_t>(0));
        return;
      }

      // 0 * x → 0
      if (binop->op == OpType::Mul && is_zero(binop->left.get())) {
        node = std::make_unique<ConstantNode>(static_cast<int64_t>(0));
        return;
      }

      // x * 1 → x
      if (binop->op == OpType::Mul && is_one(binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // 1 * x → x
      if (binop->op == OpType::Mul && is_one(binop->left.get())) {
        node = std::move(binop->right);
        return;
      }

      // x / 1 → x
      if (binop->op == OpType::Div && is_one(binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // 0 / x → 0 (assuming x != 0)
      if (binop->op == OpType::Div && is_zero(binop->left.get())) {
        node = std::make_unique<ConstantNode>(static_cast<int64_t>(0));
        return;
      }

      // x & 0 → 0
      if (binop->op == OpType::BitAnd && is_zero(binop->right.get())) {
        node = std::make_unique<ConstantNode>(static_cast<int64_t>(0));
        return;
      }

      // x | 0 → x
      if (binop->op == OpType::BitOr && is_zero(binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // x ^ 0 → x
      if (binop->op == OpType::BitXor && is_zero(binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // x & x → x
      if (binop->op == OpType::BitAnd &&
          nodes_equal(binop->left.get(), binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // x | x → x
      if (binop->op == OpType::BitOr &&
          nodes_equal(binop->left.get(), binop->right.get())) {
        node = std::move(binop->left);
        return;
      }

      // x ^ x → 0
      if (binop->op == OpType::BitXor &&
          nodes_equal(binop->left.get(), binop->right.get())) {
        node = std::make_unique<ConstantNode>(static_cast<int64_t>(0));
        return;
      }
    }
  }

  // Pass 3: Replace expensive ops with cheaper equivalents
  void strength_reduction(std::unique_ptr<ExprNode>& node) {
    if (auto* binop = dynamic_cast<BinaryOpNode*>(node.get())) {
      strength_reduction(binop->left);
      strength_reduction(binop->right);

      // x * 2 → x + x (addition is cheaper than multiplication)
      if (binop->op == OpType::Mul && is_power_of_two(binop->right.get())) {
        int power = get_power_of_two(binop->right.get());

        if (power == 1) {
          // x * 2 → x + x
          auto new_binop = std::make_unique<BinaryOpNode>();
          new_binop->op = OpType::Add;
          new_binop->left = std::move(binop->left);
          new_binop->right = new_binop->left->clone();
          new_binop->result_type = binop->result_type;
          node = std::move(new_binop);
        } else if (is_integral_type(binop->left->result_type)) {
          // x * 2^n → x << n (for integral types)
          auto shift_amount =
              std::make_unique<ConstantNode>(static_cast<int64_t>(power));
          auto new_binop = std::make_unique<BinaryOpNode>();
          new_binop->op = OpType::Shl;
          new_binop->left = std::move(binop->left);
          new_binop->right = std::move(shift_amount);
          new_binop->result_type = binop->result_type;
          node = std::move(new_binop);
        }
      }

      // x / 2^n → x >> n (for integral types)
      if (binop->op == OpType::Div &&
          is_integral_type(binop->left->result_type) &&
          is_power_of_two(binop->right.get())) {
        int power = get_power_of_two(binop->right.get());
        auto shift_amount =
            std::make_unique<ConstantNode>(static_cast<int64_t>(power));
        auto new_binop = std::make_unique<BinaryOpNode>();
        new_binop->op = OpType::Shr;
        new_binop->left = std::move(binop->left);
        new_binop->right = std::move(shift_amount);
        new_binop->result_type = binop->result_type;
        node = std::move(new_binop);
      }
    }
  }

  // Pass 4: Eliminate redundant casts
  void fold_casts(std::unique_ptr<ExprNode>& node) {
    if (auto* cast = dynamic_cast<CastNode*>(node.get())) {
      fold_casts(cast->child);

      // cast<T>(cast<U>(x)) → cast<T>(x)
      if (auto* inner = dynamic_cast<CastNode*>(cast->child.get())) {
        cast->child = std::move(inner->child);
      }

      // cast<T>(x) where x is already T → x
      if (cast->child && cast->child->result_type == cast->target_type) {
        node = std::move(cast->child);
      }
    } else if (auto* binop = dynamic_cast<BinaryOpNode*>(node.get())) {
      fold_casts(binop->left);
      fold_casts(binop->right);
    } else if (auto* unary = dynamic_cast<UnaryOpNode*>(node.get())) {
      fold_casts(unary->child);
    } else if (auto* ternary = dynamic_cast<TernaryNode*>(node.get())) {
      fold_casts(ternary->condition);
      fold_casts(ternary->true_branch);
      fold_casts(ternary->false_branch);
    }
  }

  // Pass 5: Hoist type conversions to leaves (enables fusion)
  void hoist_conversions(std::unique_ptr<ExprNode>& node) {
    if (auto* binop = dynamic_cast<BinaryOpNode*>(node.get())) {
      hoist_conversions(binop->left);
      hoist_conversions(binop->right);

      DataType target = binop->result_type;
      if (target != DataType::Unknown) {
        ensure_type(binop->left, target);
        ensure_type(binop->right, target);
      }
    } else if (auto* unary = dynamic_cast<UnaryOpNode*>(node.get())) {
      hoist_conversions(unary->child);
    } else if (auto* ternary = dynamic_cast<TernaryNode*>(node.get())) {
      hoist_conversions(ternary->condition);
      hoist_conversions(ternary->true_branch);
      hoist_conversions(ternary->false_branch);
    }
  }

  void ensure_type(std::unique_ptr<ExprNode>& node, DataType target) {
    if (!node || node->result_type == target)
      return;

    // Don't add cast if already a cast
    if (dynamic_cast<CastNode*>(node.get()))
      return;

    auto cast = std::make_unique<CastNode>(target, std::move(node));
    node = std::move(cast);
  }

  // Helper functions
  bool is_zero(const ExprNode* node) const {
    if (auto* c = dynamic_cast<const ConstantNode*>(node)) {
      return c->value.f64 == 0.0;
    }
    return false;
  }

  bool is_one(const ExprNode* node) const {
    if (auto* c = dynamic_cast<const ConstantNode*>(node)) {
      return c->value.f64 == 1.0;
    }
    return false;
  }

  bool is_power_of_two(const ExprNode* node) const {
    if (auto* c = dynamic_cast<const ConstantNode*>(node)) {
      int64_t val = c->value.i64;
      return val > 0 && (val & (val - 1)) == 0;
    }
    return false;
  }

  int get_power_of_two(const ExprNode* node) const {
    auto* c = static_cast<const ConstantNode*>(node);
    return __builtin_ctzll(c->value.u64);
  }

  bool nodes_equal(const ExprNode* a, const ExprNode* b) const {
    // Simplified equality check - only for column refs
    auto* ref_a = dynamic_cast<const ColumnRefNode*>(a);
    auto* ref_b = dynamic_cast<const ColumnRefNode*>(b);

    if (ref_a && ref_b) {
      return ref_a->name == ref_b->name;
    }

    return false;
  }

  std::unique_ptr<ConstantNode> evaluate_constant(OpType op,
                                                  const ConstantNode* lhs,
                                                  const ConstantNode* rhs) {
    // Handle integer operations
    if (is_integral_type(lhs->result_type) &&
        is_integral_type(rhs->result_type)) {
      int64_t left_val = lhs->value.i64;
      int64_t right_val = rhs->value.i64;
      int64_t result;

      switch (op) {
      case OpType::Add:
        result = left_val + right_val;
        break;
      case OpType::Sub:
        result = left_val - right_val;
        break;
      case OpType::Mul:
        result = left_val * right_val;
        break;
      case OpType::Div:
        if (right_val == 0)
          return nullptr;
        result = left_val / right_val;
        break;
      case OpType::Mod:
        if (right_val == 0)
          return nullptr;
        result = left_val % right_val;
        break;
      case OpType::BitAnd:
        result = left_val & right_val;
        break;
      case OpType::BitOr:
        result = left_val | right_val;
        break;
      case OpType::BitXor:
        result = left_val ^ right_val;
        break;
      case OpType::Shl:
        result = left_val << right_val;
        break;
      case OpType::Shr:
        result = left_val >> right_val;
        break;
      default:
        return nullptr;
      }

      return std::make_unique<ConstantNode>(result);
    }

    // Handle floating-point operations
    if (is_floating_type(lhs->result_type) ||
        is_floating_type(rhs->result_type)) {
      double left_val = lhs->value.f64;
      double right_val = rhs->value.f64;
      double result;

      switch (op) {
      case OpType::Add:
        result = left_val + right_val;
        break;
      case OpType::Sub:
        result = left_val - right_val;
        break;
      case OpType::Mul:
        result = left_val * right_val;
        break;
      case OpType::Div:
        if (right_val == 0.0)
          return nullptr;
        result = left_val / right_val;
        break;
      default:
        return nullptr;
      }

      return std::make_unique<ConstantNode>(result);
    }

    return nullptr;
  }

  std::unique_ptr<ConstantNode>
  evaluate_unary_constant(OpType op, const ConstantNode* child) {
    if (op == OpType::BitNot && is_integral_type(child->result_type)) {
      int64_t result = ~child->value.i64;
      return std::make_unique<ConstantNode>(result);
    }

    if (op == OpType::LogicalNot) {
      bool result = !child->value.i64;
      auto node = std::make_unique<ConstantNode>(result);
      return node;
    }

    return nullptr;
  }
};

} // namespace franklin::expr
