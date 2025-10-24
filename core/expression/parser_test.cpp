#include "core/expression/parser.hpp"
#include "core/math_utils.hpp"
#include "gmock/gmock.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <variant>

namespace franklin::parser {
namespace {

TEST(ParserTest, BalancedParensValidator) {
  {
    // Balanced
    const std::string str = "(())";
    ParseResult result = parse(str);
    EXPECT_TRUE(parse_result_ok(result));
  }

  {
    // Unclosed
    const std::string str = "(()";
    ParseResult result = parse(str);
    EXPECT_FALSE(parse_result_ok(result));
    auto errors = parse_result_extract_errors(result);
    EXPECT_EQ(errors.error_list.size(), 1);
    auto error = errors.error_list.front();
    EXPECT_EQ(error.pos, 0);
    EXPECT_THAT(error.desc, testing::HasSubstr("[[UnclosedScopeError]]"));
  }

  {
    // Unopened
    const std::string str = "())";
    ParseResult result = parse(str);
    EXPECT_FALSE(parse_result_ok(result));
    auto errors = parse_result_extract_errors(result);
    EXPECT_EQ(errors.error_list.size(), 1);
    auto error = errors.error_list.front();
    EXPECT_THAT(error.desc, testing::HasSubstr("[[UnopenedScopeError]]"));
  }

  {
    // Unbalanced
    const std::string str = ")";
    ParseResult result = parse(str);
    EXPECT_FALSE(parse_result_ok(result));
    auto errors = parse_result_extract_errors(result);
    EXPECT_EQ(errors.error_list.size(), 1);
    auto error = errors.error_list.front();
    EXPECT_THAT(error.desc, testing::HasSubstr("[[UnopenedScopeError]]"));
  }
}

TEST(ParserTest, LiteralValueParser) {
  {
    auto const result = LiteralNode::parse_from_data("1.0_f32");
    EXPECT_TRUE(std::holds_alternative<LiteralNode>(result));

    auto const node = std::get<LiteralNode>(result);

    float f{};
    node.visit([&f]<typename LiteralT>(LiteralT l) {
      if constexpr (std::is_same_v<LiteralT, float>) {
        f = l;
      }
    });
    EXPECT_THAT(f, testing::FloatEq(1.0));
  }
  {
    auto const result = LiteralNode::parse_from_data("2147483647_i32");
    EXPECT_TRUE(std::holds_alternative<LiteralNode>(result));

    auto const node = std::get<LiteralNode>(result);

    std::int32_t f{};
    node.visit([&f]<typename LiteralT>(LiteralT l) {
      if constexpr (std::is_same_v<LiteralT, std::int32_t>) {
        f = l;
      }
    });
    EXPECT_THAT(f, testing::Eq((1U << 31) - 1));
  }
}

TEST(ParserTest, OpPrecedence) {
  {
    const auto input_string = "a*b+c";
    auto parse_result = parse(input_string);
    EXPECT_TRUE(parse_result_ok(parse_result));
    auto const result = extract_result(std::move(parse_result));
    // std::cout << result->to_string() << std::endl;
    EXPECT_EQ(result->to_string(), "(((a)*(b))+(c))");
  }
  {
    const auto input_string = "(a*b+c)";
    auto const parse_result = parse(input_string);
    EXPECT_TRUE(parse_result_ok(parse_result));
  }

  {
    const auto input_string = "a*(b+c)";
    auto const parse_result = parse(input_string);
    EXPECT_TRUE(parse_result_ok(parse_result));
  }
  {
    const auto input_string = "(a*(b+c))";
    auto const parse_result = parse(input_string);
    EXPECT_TRUE(parse_result_ok(parse_result));
  }
  {
    const auto input_string = "(a * (    b +      c)  )";
    auto const parse_result = parse(input_string);
    EXPECT_TRUE(parse_result_ok(parse_result));
  }
}

} // namespace
} // namespace franklin::parser
