#include "core/expression/parser.hpp"
#include "core/math_utils.hpp"
#include "gmock/gmock.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

namespace franklin::parser {
namespace {

// ============================================================================
// TEST SUITE: is_pow2()
// Tests the power-of-2 detection function across different types
// ============================================================================

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

} // namespace
} // namespace franklin::parser
