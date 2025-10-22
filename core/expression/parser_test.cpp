#include "parser.hpp"
#include "type_inference.hpp"
#include <gtest/gtest.h>

namespace franklin::expr {

class ParserTest : public ::testing::Test {
protected:
  std::unordered_map<std::string, DataType> column_types{
      {"a", DataType::Int32},
      {"b", DataType::Int32},
      {"c", DataType::Float32},
      {"d", DataType::Float64},
  };
};

TEST_F(ParserTest, ParseSimpleAddition) {
  ExpressionParser parser;
  auto ast = parser.parse("a + b", column_types);

  ASSERT_NE(ast, nullptr);

  auto* binop = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(binop, nullptr);
  EXPECT_EQ(binop->op, OpType::Add);

  auto* left = dynamic_cast<ColumnRefNode*>(binop->left.get());
  auto* right = dynamic_cast<ColumnRefNode*>(binop->right.get());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->name, "a");
  EXPECT_EQ(right->name, "b");
}

TEST_F(ParserTest, ParseWithPrecedence) {
  ExpressionParser parser;
  auto ast = parser.parse("a + b * c", column_types);

  ASSERT_NE(ast, nullptr);

  // Should be: a + (b * c)
  auto* add = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->op, OpType::Add);

  auto* left = dynamic_cast<ColumnRefNode*>(add->left.get());
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->name, "a");

  auto* mul = dynamic_cast<BinaryOpNode*>(add->right.get());
  ASSERT_NE(mul, nullptr);
  EXPECT_EQ(mul->op, OpType::Mul);
}

TEST_F(ParserTest, ParseWithParentheses) {
  ExpressionParser parser;
  auto ast = parser.parse("(a + b) * c", column_types);

  ASSERT_NE(ast, nullptr);

  // Should be: (a + b) * c
  auto* mul = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(mul, nullptr);
  EXPECT_EQ(mul->op, OpType::Mul);

  auto* add = dynamic_cast<BinaryOpNode*>(mul->left.get());
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->op, OpType::Add);

  auto* right = dynamic_cast<ColumnRefNode*>(mul->right.get());
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->name, "c");
}

TEST_F(ParserTest, ParseNumberLiteral) {
  ExpressionParser parser;
  auto ast = parser.parse("42", column_types);

  ASSERT_NE(ast, nullptr);

  auto* constant = dynamic_cast<ConstantNode*>(ast.get());
  ASSERT_NE(constant, nullptr);
  EXPECT_EQ(constant->value.i64, 42);
}

TEST_F(ParserTest, ParseFloatLiteral) {
  ExpressionParser parser;
  auto ast = parser.parse("3.14", column_types);

  ASSERT_NE(ast, nullptr);

  auto* constant = dynamic_cast<ConstantNode*>(ast.get());
  ASSERT_NE(constant, nullptr);
  EXPECT_DOUBLE_EQ(constant->value.f64, 3.14);
}

TEST_F(ParserTest, ParseBitwiseOperations) {
  ExpressionParser parser;
  auto ast = parser.parse("a & b | c", column_types);

  ASSERT_NE(ast, nullptr);

  // Should be: (a & b) | c (& has higher precedence than |)
  auto* or_op = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(or_op, nullptr);
  EXPECT_EQ(or_op->op, OpType::BitOr);

  auto* and_op = dynamic_cast<BinaryOpNode*>(or_op->left.get());
  ASSERT_NE(and_op, nullptr);
  EXPECT_EQ(and_op->op, OpType::BitAnd);
}

TEST_F(ParserTest, ParseComparison) {
  ExpressionParser parser;
  auto ast = parser.parse("a < b", column_types);

  ASSERT_NE(ast, nullptr);

  auto* cmp = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(cmp, nullptr);
  EXPECT_EQ(cmp->op, OpType::Lt);
}

TEST_F(ParserTest, ParseLogicalOperations) {
  ExpressionParser parser;
  auto ast = parser.parse("a && b || c", column_types);

  ASSERT_NE(ast, nullptr);

  // Should be: (a && b) || c (&& has higher precedence)
  auto* or_op = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(or_op, nullptr);
  EXPECT_EQ(or_op->op, OpType::LogicalOr);

  auto* and_op = dynamic_cast<BinaryOpNode*>(or_op->left.get());
  ASSERT_NE(and_op, nullptr);
  EXPECT_EQ(and_op->op, OpType::LogicalAnd);
}

TEST_F(ParserTest, ParseTypeCast) {
  ExpressionParser parser;
  auto ast = parser.parse("float32(a)", column_types);

  ASSERT_NE(ast, nullptr);

  auto* cast = dynamic_cast<CastNode*>(ast.get());
  ASSERT_NE(cast, nullptr);
  EXPECT_EQ(cast->target_type, DataType::Float32);

  auto* child = dynamic_cast<ColumnRefNode*>(cast->child.get());
  ASSERT_NE(child, nullptr);
  EXPECT_EQ(child->name, "a");
}

TEST_F(ParserTest, ParseUnaryMinus) {
  ExpressionParser parser;
  auto ast = parser.parse("-a", column_types);

  ASSERT_NE(ast, nullptr);

  // Unary minus is converted to: 0 - a
  auto* sub = dynamic_cast<BinaryOpNode*>(ast.get());
  ASSERT_NE(sub, nullptr);
  EXPECT_EQ(sub->op, OpType::Sub);

  auto* zero = dynamic_cast<ConstantNode*>(sub->left.get());
  ASSERT_NE(zero, nullptr);
  EXPECT_DOUBLE_EQ(zero->value.f64, 0.0);
}

TEST_F(ParserTest, ParseUnaryNot) {
  ExpressionParser parser;
  auto ast = parser.parse("~a", column_types);

  ASSERT_NE(ast, nullptr);

  auto* unary = dynamic_cast<UnaryOpNode*>(ast.get());
  ASSERT_NE(unary, nullptr);
  EXPECT_EQ(unary->op, OpType::BitNot);
}

TEST_F(ParserTest, ParseTernary) {
  ExpressionParser parser;
  auto ast = parser.parse("a > 0 ? b : c", column_types);

  ASSERT_NE(ast, nullptr);

  auto* ternary = dynamic_cast<TernaryNode*>(ast.get());
  ASSERT_NE(ternary, nullptr);

  auto* cond = dynamic_cast<BinaryOpNode*>(ternary->condition.get());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->op, OpType::Gt);

  auto* true_branch = dynamic_cast<ColumnRefNode*>(ternary->true_branch.get());
  ASSERT_NE(true_branch, nullptr);
  EXPECT_EQ(true_branch->name, "b");

  auto* false_branch =
      dynamic_cast<ColumnRefNode*>(ternary->false_branch.get());
  ASSERT_NE(false_branch, nullptr);
  EXPECT_EQ(false_branch->name, "c");
}

TEST_F(ParserTest, TypeInferenceBasic) {
  ExpressionParser parser;
  auto ast = parser.parse("a + b", column_types);

  TypeInference inference;
  inference.infer(ast.get());

  // a and b are both int32, result should be int32
  EXPECT_EQ(ast->result_type, DataType::Int32);
}

TEST_F(ParserTest, TypeInferencePromotion) {
  ExpressionParser parser;
  auto ast = parser.parse("a + c", column_types);

  TypeInference inference;
  inference.infer(ast.get());

  // a is int32, c is float32, result should be promoted to float32
  EXPECT_EQ(ast->result_type, DataType::Float32);
}

TEST_F(ParserTest, TypeInferenceNested) {
  ExpressionParser parser;
  auto ast = parser.parse("a + b * c", column_types);

  TypeInference inference;
  inference.infer(ast.get());

  // a:int32, b:int32, c:float32
  // b * c → float32
  // a + float32 → float32
  EXPECT_EQ(ast->result_type, DataType::Float32);
}

TEST_F(ParserTest, TypeInferenceComparison) {
  ExpressionParser parser;
  auto ast = parser.parse("a < b", column_types);

  TypeInference inference;
  inference.infer(ast.get());

  // Comparison should return bool
  EXPECT_EQ(ast->result_type, DataType::Bool);
}

TEST_F(ParserTest, ComplexExpression) {
  ExpressionParser parser;
  auto ast = parser.parse("(a + b) * c - d / 2.0", column_types);

  ASSERT_NE(ast, nullptr);

  TypeInference inference;
  inference.infer(ast.get());

  // Should successfully parse and infer types
  EXPECT_NE(ast->result_type, DataType::Unknown);
}

} // namespace franklin::expr
