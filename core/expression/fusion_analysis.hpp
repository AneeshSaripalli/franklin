#pragma once

#include "ast.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace franklin::expr {

class FusionAnalyzer {
public:
  struct FusionOpportunity {
    ExprNode* root = nullptr;           // Root of fusion group
    std::vector<const ExprNode*> nodes; // All nodes to fuse
    size_t input_columns = 0;           // Memory loads required
    size_t output_columns = 1;          // Usually 1
    uint8_t register_pressure = 0;      // SIMD registers needed
    float speedup_estimate = 1.0f;      // Estimated perf gain
    bool fits_in_tier0 = false;         // Pre-compiled kernel exists
    bool fits_in_tier1 = false;         // Can template instantiate
    bool needs_tier2 = false;           // Requires codegen
    ExprPattern pattern = ExprPattern::ComplexUnfusible;
  };

  std::vector<FusionOpportunity> analyze(ExprNode* root) {
    std::vector<FusionOpportunity> opportunities;

    if (!root)
      return opportunities;

    // For now, analyze the entire tree as one potential fusion
    FusionOpportunity opp = evaluate_fusion(root);

    if (should_fuse(opp)) {
      opportunities.push_back(opp);
    }

    return opportunities;
  }

private:
  FusionOpportunity evaluate_fusion(ExprNode* root) {
    FusionOpportunity opp;
    opp.root = root;

    // Collect all nodes in the tree
    collect_nodes(root, opp.nodes);

    // Count input columns (ColumnRef nodes)
    opp.input_columns = count_inputs(opp.nodes);

    // Estimate register pressure
    // Simple model: one register per input + one per intermediate
    opp.register_pressure =
        static_cast<uint8_t>(opp.input_columns + opp.nodes.size());

    // Classify pattern
    opp.pattern = classify_pattern(root);

    // Determine which tier fits
    opp.fits_in_tier0 = matches_tier0_pattern(opp.pattern, root);
    opp.fits_in_tier1 = opp.nodes.size() <= 8 && is_simple_expression(root);
    opp.needs_tier2 = !opp.fits_in_tier0 && !opp.fits_in_tier1;

    // Estimate speedup from fusion
    size_t intermediates = opp.nodes.size() > 1 ? opp.nodes.size() - 1 : 0;

    if (intermediates > 0) {
      // Assume 4MB columns, 50GB/s bandwidth
      constexpr size_t column_bytes = 1000000 * 4; // 1M elements * 4 bytes
      constexpr double bandwidth_gbs = 50.0;

      double memory_time_saved_ms =
          (intermediates * column_bytes) / (bandwidth_gbs * 1e9 / 1000.0);

      // Assume compute takes ~1ms per operation
      double compute_time_ms = opp.nodes.size() * 0.5;

      opp.speedup_estimate =
          (compute_time_ms + memory_time_saved_ms) / compute_time_ms;
    }

    return opp;
  }

  bool should_fuse(const FusionOpportunity& opp) {
    // Don't fuse if register pressure too high
    // AVX2 has 16 YMM registers, leave some for temporaries
    if (opp.register_pressure > 12) {
      return false;
    }

    // Must have at least 1.2x speedup
    if (opp.speedup_estimate < 1.2f) {
      return false;
    }

    // Don't fuse single operations
    if (opp.nodes.size() < 2) {
      return false;
    }

    // Don't fuse if contains unfusible nodes
    for (auto* node : opp.nodes) {
      if (!node->can_fuse) {
        return false;
      }
    }

    return true;
  }

  void collect_nodes(const ExprNode* node,
                     std::vector<const ExprNode*>& nodes) {
    if (!node)
      return;

    nodes.push_back(node);

    if (auto* binop = dynamic_cast<const BinaryOpNode*>(node)) {
      collect_nodes(binop->left.get(), nodes);
      collect_nodes(binop->right.get(), nodes);
    } else if (auto* unary = dynamic_cast<const UnaryOpNode*>(node)) {
      collect_nodes(unary->child.get(), nodes);
    } else if (auto* cast = dynamic_cast<const CastNode*>(node)) {
      collect_nodes(cast->child.get(), nodes);
    } else if (auto* ternary = dynamic_cast<const TernaryNode*>(node)) {
      collect_nodes(ternary->condition.get(), nodes);
      collect_nodes(ternary->true_branch.get(), nodes);
      collect_nodes(ternary->false_branch.get(), nodes);
    }
  }

  size_t count_inputs(const std::vector<const ExprNode*>& nodes) {
    size_t count = 0;
    for (auto* node : nodes) {
      if (dynamic_cast<const ColumnRefNode*>(node)) {
        count++;
      }
    }
    return count;
  }

  ExprPattern classify_pattern(const ExprNode* root) {
    // Single column reference
    if (dynamic_cast<const ColumnRefNode*>(root)) {
      return ExprPattern::SingleColumn;
    }

    // Check for FMA pattern: a * b + c
    if (auto* add = dynamic_cast<const BinaryOpNode*>(root)) {
      if (add->op == OpType::Add) {
        auto* left = add->left.get();
        if (auto* mul = dynamic_cast<const BinaryOpNode*>(left)) {
          if (mul->op == OpType::Mul) {
            if (is_column_ref(mul->left.get()) &&
                is_column_ref(mul->right.get()) &&
                is_column_ref(add->right.get())) {
              return ExprPattern::FMA;
            }
          }
        }
      }
    }

    // Binary operation: a op b
    if (auto* binop = dynamic_cast<const BinaryOpNode*>(root)) {
      if (is_column_ref(binop->left.get()) &&
          is_column_ref(binop->right.get())) {
        return ExprPattern::BinaryOp;
      }

      // Nested binary: a + (b * c) or (a + b) * c
      if ((is_column_ref(binop->left.get()) &&
           is_binary_of_refs(binop->right.get())) ||
          (is_binary_of_refs(binop->left.get()) &&
           is_column_ref(binop->right.get()))) {
        return ExprPattern::NestedBinary;
      }

      // More complex but still fusible
      if (count_operations(root) <= 8) {
        return ExprPattern::ComplexFusible;
      }
    }

    return ExprPattern::ComplexUnfusible;
  }

  bool matches_tier0_pattern(ExprPattern pattern, const ExprNode* root) {
    // Tier 0 has pre-compiled kernels for these patterns
    switch (pattern) {
    case ExprPattern::SingleColumn:
    case ExprPattern::BinaryOp:
    case ExprPattern::NestedBinary:
    case ExprPattern::FMA:
      return true;
    default:
      return false;
    }
  }

  bool is_simple_expression(const ExprNode* root) {
    // Simple means: only binary ops, no ternary, no complex control flow
    if (dynamic_cast<const TernaryNode*>(root)) {
      return false;
    }

    // Recursively check children
    if (auto* binop = dynamic_cast<const BinaryOpNode*>(root)) {
      return is_simple_expression(binop->left.get()) &&
             is_simple_expression(binop->right.get());
    }

    if (auto* unary = dynamic_cast<const UnaryOpNode*>(root)) {
      return is_simple_expression(unary->child.get());
    }

    if (auto* cast = dynamic_cast<const CastNode*>(root)) {
      return is_simple_expression(cast->child.get());
    }

    return true; // Leaf nodes are simple
  }

  bool is_column_ref(const ExprNode* node) const {
    return dynamic_cast<const ColumnRefNode*>(node) != nullptr;
  }

  bool is_binary_of_refs(const ExprNode* node) const {
    if (auto* binop = dynamic_cast<const BinaryOpNode*>(node)) {
      return is_column_ref(binop->left.get()) &&
             is_column_ref(binop->right.get());
    }
    return false;
  }

  size_t count_operations(const ExprNode* node) const {
    if (!node)
      return 0;

    if (auto* binop = dynamic_cast<const BinaryOpNode*>(node)) {
      return 1 + count_operations(binop->left.get()) +
             count_operations(binop->right.get());
    }

    if (auto* unary = dynamic_cast<const UnaryOpNode*>(node)) {
      return 1 + count_operations(unary->child.get());
    }

    if (auto* ternary = dynamic_cast<const TernaryNode*>(node)) {
      return 1 + count_operations(ternary->condition.get()) +
             count_operations(ternary->true_branch.get()) +
             count_operations(ternary->false_branch.get());
    }

    return 0; // Leaf nodes (no operation)
  }
};

} // namespace franklin::expr
