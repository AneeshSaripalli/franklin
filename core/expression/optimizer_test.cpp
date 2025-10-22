#include "optimizer.hpp"
#include "parser.hpp"
#include "type_inference.hpp"
#include <gtest/gtest.h>

namespace franklin::expr {

class OptimizerTest : public ::testing::Test {
protected:
  std::unordered_map<std::string, DataType> column_types{
      {"a", DataType::Int32},
      {"b", DataType::Int32},
      {"c", DataType::Float32},
  };

  std::unique_ptr<ExprNode> parse_and_optimize(const std::string& expr) {
    ExpressionParser parser;
    auto ast = parser.parse(expr, column_types);

    TypeInference inference;
    inference.infer(ast.get());

    ExpressionOptimizer optimizer;
    optimizer.optimize(ast);

    return ast;
  }
};

TEST_F(OptimizerTest, ConstantFolding) {
  auto ast = parse_and_optimize("2 + 3");

  // Should be folded to constant 5
  auto* constant = dynamic_cast<ConstantNode*>(ast.get());
  ASSERT_NE(constant, nullptr);
  EXPECT_EQ(constant->value.i64, 5);
}

TEST_F(OptimizerTest, ConstantFoldingMultiplication) {
  auto ast = parse_and_optimize("4 * 5");

  auto* constant = dynamic_cast<ConstantNode*>(ast.get());
  ASSERT_NE(constant, nullptr);
  EXPECT_EQ(constant->value.i64, 20);
}

TEST_F(OptimizerTest, ConstantFoldingFloat) {
  auto ast = parse_and_optimize("2.5 + 1.5");

  auto* constant = dynamic_cast<ConstantNode*>(ast.get());
  ASSERT_NE(constant, nullptr);
  EXPECT_DOUBLE_EQ(constant->value.f64, 4.0);
}

TEST_F(OptimizerTest, AlgebraicSimplification_AddZero) {
  auto ast = parse_and_optimize("a + 0");

  // Should be simplified to just 'a'
  auto* ref = dynamic_cast<ColumnRefNode*>(ast.get());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->name, "a");
}

TEST_F(OptimizerTest, AlgebraicSimplification_MulOne) {
  auto ast = parse_and_optimize("a * 1");

  auto* ref = dynamic_cast<ColumnRefNode*>(ast.get());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->name, "a");
}

TEST_F(OptimizerTest, AlgebraicSimplification_MulZero) {
  auto ast = parse_and_optimize("a * 0");

  auto* constant = dynamic_cast<ConstantNode*>(ast.get());
  ASSERT_NE(constant, nullptr);
  EXPECT_EQ(constant->value.i64, static_cast<int64_t>(0));
}

TEST_F(OptimizerTest, AlgebraicSimplification_DivOne) {
  auto ast = parse_and_optimize("a / 1");

  auto* ref = dynamic_cast<ColumnRefNode*>(ast.get());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->name, "a");
}

TEST_F(OptimizerTest, StrengthReduction_MulByTwo) {
  auto ast = parse_and_optimize("a * 2");

  // Should be converted to a + a
  auto* binop = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(binop, nullptr);
  EXPECT_EQ(binop->op, OpType::Add);

  auto* left = dynamic_cast<ColumnRefNode*>(binop->left.get());
  auto* right = dynamic_cast<ColumnRefNode*>(binop->right.get());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->name, "a");
  EXPECT_EQ(right->name, "a");
}

TEST_F(OptimizerTest, StrengthReduction_MulByPowerOfTwo) {
  auto ast = parse_and_optimize("a * 8");

  // Should be converted to a << 3
  auto* binop = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(binop, nullptr);
  EXPECT_EQ(binop->op, OpType::Shl);

  auto* shift_amount = dynamic_cast<ConstantNode*>(binop->right.get());
  ASSERT_NE(shift_amount, nullptr);
  EXPECT_EQ(shift_amount->value.i64, 3); // 8 = 2^3
}

TEST_F(OptimizerTest, StrengthReduction_DivByPowerOfTwo) {
  auto ast = parse_and_optimize("a / 4");

  // Should be converted to a >> 2
  auto* binop = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(binop, nullptr);
  EXPECT_EQ(binop->op, OpType::Shr);

  auto* shift_amount = dynamic_cast<ConstantNode*>(binop->right.get());
  ASSERT_NE(shift_amount, nullptr);
  EXPECT_EQ(shift_amount->value.i64, 2); // 4 = 2^2
}

TEST_F(OptimizerTest, CastElimination_RedundantCast) {
  // Parse manually to create a redundant cast
  auto ref = std::make_unique<ColumnRefNode>("a");
  ref->result_type = DataType::Int32;

  auto cast1 = std::make_unique<CastNode>(DataType::Float32, std::move(ref));
  auto cast2 = std::make_unique<CastNode>(DataType::Int32, std::move(cast1));

  ExpressionOptimizer optimizer;
  std::unique_ptr<ExprNode> ast = std::move(cast2);
  optimizer.optimize(ast);

  // Should eliminate the redundant cast back to int32
  auto* final_cast = dynamic_cast<CastNode*>(ast.get());
  ASSERT_NE(final_cast, nullptr);
  EXPECT_EQ(final_cast->target_type, DataType::Int32);

  // The child should be the original ColumnRef, not another cast
  auto* child_ref = dynamic_cast<ColumnRefNode*>(final_cast->child.get());
  ASSERT_NE(child_ref, nullptr);
}

TEST_F(OptimizerTest, ComplexOptimization) {
  // a * 1 + 0 * b should become just 'a'
  auto ast = parse_and_optimize("a * 1 + 0 * b");

  auto* ref = dynamic_cast<ColumnRefNode*>(ast.get());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->name, "a");
}

TEST_F(OptimizerTest, BitwiseSimplification_XorSelf) {
  auto ast = parse_and_optimize("a ^ a");

  auto* constant = dynamic_cast<ConstantNode*>(ast.get());
  ASSERT_NE(constant, nullptr);
  EXPECT_EQ(constant->value.i64, static_cast<int64_t>(0));
}

TEST_F(OptimizerTest, BitwiseSimplification_OrSelf) {
  auto ast = parse_and_optimize("a | a");

  auto* ref = dynamic_cast<ColumnRefNode*>(ast.get());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->name, "a");
}

TEST_F(OptimizerTest, BitwiseSimplification_AndSelf) {
  auto ast = parse_and_optimize("a & a");

  auto* ref = dynamic_cast<ColumnRefNode*>(ast.get());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->name, "a");
}

} // namespace franklin::expr
