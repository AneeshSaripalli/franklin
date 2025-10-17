#include "ast.hpp"
#include <gtest/gtest.h>

namespace franklin::expr {

TEST(ASTTest, ColumnRefNodeCreation) {
  ColumnRefNode node("test_column", 0);
  EXPECT_EQ(node.name, "test_column");
  EXPECT_EQ(node.column_index, 0);
  EXPECT_EQ(node.complexity, 0);
  EXPECT_EQ(node.memory_loads, 1);
}

TEST(ASTTest, ConstantNodeInt) {
  ConstantNode node(static_cast<int64_t>(42));
  EXPECT_EQ(node.value.i64, 42);
  EXPECT_EQ(node.result_type, DataType::Int64);
  EXPECT_TRUE(node.is_constant);
}

TEST(ASTTest, ConstantNodeFloat) {
  ConstantNode node(3.14);
  EXPECT_DOUBLE_EQ(node.value.f64, 3.14);
  EXPECT_EQ(node.result_type, DataType::Float64);
}

TEST(ASTTest, BinaryOpNodeCreation) {
  auto left = std::make_unique<ConstantNode>(static_cast<int64_t>(1));
  auto right = std::make_unique<ConstantNode>(static_cast<int64_t>(2));

  BinaryOpNode node(OpType::Add, std::move(left), std::move(right));
  EXPECT_EQ(node.op, OpType::Add);
  EXPECT_NE(node.left, nullptr);
  EXPECT_NE(node.right, nullptr);
  EXPECT_EQ(node.complexity, 1); // 1 operation
}

TEST(ASTTest, CloneColumnRef) {
  ColumnRefNode original("test", 5);
  original.result_type = DataType::Float32;

  auto cloned = original.clone();
  auto* cloned_ref = dynamic_cast<ColumnRefNode*>(cloned.get());

  ASSERT_NE(cloned_ref, nullptr);
  EXPECT_EQ(cloned_ref->name, "test");
  EXPECT_EQ(cloned_ref->column_index, 5);
  EXPECT_EQ(cloned_ref->result_type, DataType::Float32);
}

TEST(ASTTest, CloneBinaryOp) {
  auto left = std::make_unique<ConstantNode>(static_cast<int64_t>(10));
  auto right = std::make_unique<ConstantNode>(static_cast<int64_t>(20));

  BinaryOpNode original(OpType::Mul, std::move(left), std::move(right));
  original.result_type = DataType::Int32;

  auto cloned = original.clone();
  auto* cloned_binop = dynamic_cast<BinaryOpNode*>(cloned.get());

  ASSERT_NE(cloned_binop, nullptr);
  EXPECT_EQ(cloned_binop->op, OpType::Mul);
  EXPECT_NE(cloned_binop->left, nullptr);
  EXPECT_NE(cloned_binop->right, nullptr);
  EXPECT_EQ(cloned_binop->result_type, DataType::Int32);
}

TEST(ASTTest, TypeHelpers) {
  EXPECT_TRUE(is_floating_type(DataType::Float32));
  EXPECT_TRUE(is_floating_type(DataType::Float64));
  EXPECT_TRUE(is_floating_type(DataType::BF16));
  EXPECT_FALSE(is_floating_type(DataType::Int32));

  EXPECT_TRUE(is_integral_type(DataType::Int32));
  EXPECT_TRUE(is_integral_type(DataType::UInt64));
  EXPECT_FALSE(is_integral_type(DataType::Float32));

  EXPECT_TRUE(is_bitwise_op(OpType::BitAnd));
  EXPECT_TRUE(is_bitwise_op(OpType::Shl));
  EXPECT_FALSE(is_bitwise_op(OpType::Add));

  EXPECT_TRUE(is_comparison_op(OpType::Lt));
  EXPECT_TRUE(is_comparison_op(OpType::Eq));
  EXPECT_FALSE(is_comparison_op(OpType::Add));
}

TEST(ASTTest, SizeofType) {
  EXPECT_EQ(sizeof_type(DataType::Int8), 1);
  EXPECT_EQ(sizeof_type(DataType::Int16), 2);
  EXPECT_EQ(sizeof_type(DataType::Int32), 4);
  EXPECT_EQ(sizeof_type(DataType::Int64), 8);
  EXPECT_EQ(sizeof_type(DataType::Float32), 4);
  EXPECT_EQ(sizeof_type(DataType::Float64), 8);
}

} // namespace franklin::expr
