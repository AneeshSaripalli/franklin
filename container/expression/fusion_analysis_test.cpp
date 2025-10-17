#include "fusion_analysis.hpp"
#include "parser.hpp"
#include "type_inference.hpp"
#include <gtest/gtest.h>

namespace franklin::expr {

class FusionAnalysisTest : public ::testing::Test {
protected:
  std::unordered_map<std::string, DataType> column_types{
      {"a", DataType::Float32},
      {"b", DataType::Float32},
      {"c", DataType::Float32},
      {"d", DataType::Float32},
  };

  std::unique_ptr<ExprNode> parse_expr(const std::string& expr) {
    ExpressionParser parser;
    auto ast = parser.parse(expr, column_types);

    TypeInference inference;
    inference.infer(ast.get());

    return ast;
  }
};

TEST_F(FusionAnalysisTest, SingleColumn_NoFusion) {
  auto ast = parse_expr("a");

  FusionAnalyzer analyzer;
  auto opportunities = analyzer.analyze(ast.get());

  // Single column should not be fused (less than 2 nodes)
  EXPECT_TRUE(opportunities.empty());
}

TEST_F(FusionAnalysisTest, BinaryOp_Pattern) {
  auto ast = parse_expr("a + b");

  FusionAnalyzer analyzer;
  auto opportunities = analyzer.analyze(ast.get());

  ASSERT_FALSE(opportunities.empty());

  auto& opp = opportunities[0];
  EXPECT_EQ(opp.pattern, ExprPattern::BinaryOp);
  EXPECT_EQ(opp.input_columns, 2);
  EXPECT_TRUE(opp.fits_in_tier0);
}

TEST_F(FusionAnalysisTest, NestedBinary_Pattern) {
  auto ast = parse_expr("a + b * c");

  FusionAnalyzer analyzer;
  auto opportunities = analyzer.analyze(ast.get());

  ASSERT_FALSE(opportunities.empty());

  auto& opp = opportunities[0];
  EXPECT_EQ(opp.pattern, ExprPattern::NestedBinary);
  EXPECT_EQ(opp.input_columns, 3);
  EXPECT_TRUE(opp.fits_in_tier0);
}

TEST_F(FusionAnalysisTest, FMA_Pattern) {
  auto ast = parse_expr("a * b + c");

  FusionAnalyzer analyzer;
  auto opportunities = analyzer.analyze(ast.get());

  ASSERT_FALSE(opportunities.empty());

  auto& opp = opportunities[0];
  EXPECT_EQ(opp.pattern, ExprPattern::FMA);
  EXPECT_EQ(opp.input_columns, 3);
  EXPECT_TRUE(opp.fits_in_tier0);
}

TEST_F(FusionAnalysisTest, ComplexExpression) {
  auto ast = parse_expr("(a + b) * (c + d)");

  FusionAnalyzer analyzer;
  auto opportunities = analyzer.analyze(ast.get());

  ASSERT_FALSE(opportunities.empty());

  auto& opp = opportunities[0];
  EXPECT_EQ(opp.input_columns, 4);
  // This is more complex, might need Tier 1 or 2
  EXPECT_TRUE(opp.fits_in_tier1 || opp.needs_tier2);
}

TEST_F(FusionAnalysisTest, RegisterPressureEstimate) {
  auto ast = parse_expr("a + b * c");

  FusionAnalyzer analyzer;
  auto opportunities = analyzer.analyze(ast.get());

  ASSERT_FALSE(opportunities.empty());

  auto& opp = opportunities[0];
  // 3 inputs + some intermediates
  EXPECT_GT(opp.register_pressure, 0);
  EXPECT_LT(opp.register_pressure, 16); // Should be reasonable
}

TEST_F(FusionAnalysisTest, SpeedupEstimate) {
  auto ast = parse_expr("a + b * c - d");

  FusionAnalyzer analyzer;
  auto opportunities = analyzer.analyze(ast.get());

  ASSERT_FALSE(opportunities.empty());

  auto& opp = opportunities[0];
  // Should estimate speedup > 1.0
  EXPECT_GT(opp.speedup_estimate, 1.0f);
}

} // namespace franklin::expr
