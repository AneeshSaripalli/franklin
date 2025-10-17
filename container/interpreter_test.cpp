#include "container/interpreter.hpp"
#include <gtest/gtest.h>
#include <memory>

namespace franklin {
namespace {

// Simple test policy
struct TestPolicy {
  using value_type = int;
  using allocator_type = std::allocator<int>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = false;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = false;
};

using TestInterpreter = interpreter<TestPolicy>;
using TestColumn = column_vector<TestPolicy>;

TEST(InterpreterTest, RegisterAndRetrieve) {
  TestInterpreter interp;
  TestColumn col1(10);

  interp.register_column("a", col1);

  EXPECT_TRUE(interp.has_column("a"));
  EXPECT_FALSE(interp.has_column("b"));
  EXPECT_EQ(interp.size(), 1);
}

TEST(InterpreterTest, RegisterMultipleColumns) {
  TestInterpreter interp;
  TestColumn col1(10);
  TestColumn col2(20);
  TestColumn col3(30);

  interp.register_column("a", col1);
  interp.register_column("b", col2);
  interp.register_column("c", col3);

  EXPECT_TRUE(interp.has_column("a"));
  EXPECT_TRUE(interp.has_column("b"));
  EXPECT_TRUE(interp.has_column("c"));
  EXPECT_EQ(interp.size(), 3);
}

TEST(InterpreterTest, RegisterWithMove) {
  TestInterpreter interp;
  TestColumn col1(10);

  interp.register_column("a", std::move(col1));

  EXPECT_TRUE(interp.has_column("a"));
  EXPECT_EQ(interp.size(), 1);
}

TEST(InterpreterTest, UnregisterColumn) {
  TestInterpreter interp;
  TestColumn col1(10);

  interp.register_column("a", col1);
  EXPECT_TRUE(interp.has_column("a"));

  interp.unregister_column("a");
  EXPECT_FALSE(interp.has_column("a"));
  EXPECT_EQ(interp.size(), 0);
}

TEST(InterpreterTest, Clear) {
  TestInterpreter interp;
  TestColumn col1(10);
  TestColumn col2(20);

  interp.register_column("a", col1);
  interp.register_column("b", col2);
  EXPECT_EQ(interp.size(), 2);

  interp.clear();
  EXPECT_EQ(interp.size(), 0);
  EXPECT_FALSE(interp.has_column("a"));
  EXPECT_FALSE(interp.has_column("b"));
}

TEST(InterpreterTest, GetColumnThrowsOnUnknown) {
  TestInterpreter interp;

  EXPECT_THROW(interp.get_column("unknown"), std::runtime_error);
}

TEST(InterpreterTest, EvalSimpleVariable) {
  TestInterpreter interp;
  TestColumn col1(10);

  interp.register_column("a", col1);

  // Simple variable lookup should work
  TestColumn result = interp.eval("a");
  // Can't verify much without column_vector accessors, but shouldn't throw
}

TEST(InterpreterTest, EvalThrowsOnUnknownVariable) {
  TestInterpreter interp;

  EXPECT_THROW(interp.eval("unknown"), std::runtime_error);
}

TEST(InterpreterTest, EvalBinaryExpressionThrows) {
  TestInterpreter interp;
  TestColumn col1(10);
  TestColumn col2(20);

  interp.register_column("a", col1);
  interp.register_column("b", col2);

  // Operations not implemented yet, should throw
  EXPECT_THROW(interp.eval("a + b"), std::runtime_error);
}

TEST(InterpreterTest, TokenizerHandlesWhitespace) {
  TestInterpreter interp;
  TestColumn col1(10);

  interp.register_column("a", col1);

  // Should handle various whitespace
  EXPECT_NO_THROW(interp.eval("  a  "));
  EXPECT_NO_THROW(interp.eval("a"));
  EXPECT_NO_THROW(interp.eval("\ta\n"));
}

TEST(InterpreterTest, OverwriteExistingColumn) {
  TestInterpreter interp;
  TestColumn col1(10);
  TestColumn col2(20);

  interp.register_column("a", col1);
  EXPECT_EQ(interp.size(), 1);

  // Overwrite with same name
  interp.register_column("a", col2);
  EXPECT_EQ(interp.size(), 1);
  EXPECT_TRUE(interp.has_column("a"));
}

} // namespace
} // namespace franklin
