#include "type_inference.hpp"
#include <gtest/gtest.h>

namespace franklin::expr {

TEST(TypeInferenceTest, ColumnRefNoChange) {
  ColumnRefNode node("test", 0);
  node.result_type = DataType::Int32;

  TypeInference inference;
  inference.infer(&node);

  EXPECT_EQ(node.result_type, DataType::Int32);
}

TEST(TypeInferenceTest, BinaryOpIntPromotion) {
  auto left = std::make_unique<ColumnRefNode>("a", 0);
  left->result_type = DataType::Int8;

  auto right = std::make_unique<ColumnRefNode>("b", 1);
  right->result_type = DataType::Int32;

  BinaryOpNode node(OpType::Add, std::move(left), std::move(right));

  TypeInference inference;
  inference.infer(&node);

  EXPECT_EQ(node.result_type, DataType::Int32);
}

TEST(TypeInferenceTest, BinaryOpFloatPromotion) {
  auto left = std::make_unique<ColumnRefNode>("a", 0);
  left->result_type = DataType::Int32;

  auto right = std::make_unique<ColumnRefNode>("b", 1);
  right->result_type = DataType::Float32;

  BinaryOpNode node(OpType::Add, std::move(left), std::move(right));

  TypeInference inference;
  inference.infer(&node);

  EXPECT_EQ(node.result_type, DataType::Float32);
}

TEST(TypeInferenceTest, ComparisonReturnsBool) {
  auto left = std::make_unique<ColumnRefNode>("a", 0);
  left->result_type = DataType::Int32;

  auto right = std::make_unique<ColumnRefNode>("b", 1);
  right->result_type = DataType::Int32;

  BinaryOpNode node(OpType::Lt, std::move(left), std::move(right));

  TypeInference inference;
  inference.infer(&node);

  EXPECT_EQ(node.result_type, DataType::Bool);
}

TEST(TypeInferenceTest, BitwiseRequiresIntegral) {
  auto left = std::make_unique<ColumnRefNode>("a", 0);
  left->result_type = DataType::Float32;

  auto right = std::make_unique<ColumnRefNode>("b", 1);
  right->result_type = DataType::Int32;

  BinaryOpNode node(OpType::BitAnd, std::move(left), std::move(right));

  TypeInference inference;
  EXPECT_THROW(inference.infer(&node), std::runtime_error);
}

TEST(TypeInferenceTest, CastSetsTargetType) {
  auto child = std::make_unique<ColumnRefNode>("a", 0);
  child->result_type = DataType::Int32;

  CastNode node(DataType::Float64, std::move(child));

  TypeInference inference;
  inference.infer(&node);

  EXPECT_EQ(node.result_type, DataType::Float64);
}

TEST(TypeInferenceTest, TernaryPromotion) {
  auto cond = std::make_unique<ConstantNode>(true);
  cond->result_type = DataType::Bool;

  auto true_branch = std::make_unique<ColumnRefNode>("a", 0);
  true_branch->result_type = DataType::Int32;

  auto false_branch = std::make_unique<ColumnRefNode>("b", 1);
  false_branch->result_type = DataType::Float32;

  TernaryNode node(std::move(cond), std::move(true_branch),
                   std::move(false_branch));

  TypeInference inference;
  inference.infer(&node);

  // Should promote to Float32
  EXPECT_EQ(node.result_type, DataType::Float32);
}

} // namespace franklin::expr
