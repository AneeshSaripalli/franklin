#include "core/math_utils.hpp"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

namespace franklin {
namespace {

// ============================================================================
// TEST SUITE: is_pow2()
// Tests the power-of-2 detection function across different types
// ============================================================================

TEST(IsPow2Test, ZeroIsNotPowerOfTwo) {
  // CRITICAL BUG: is_pow2(0) returns true, but 0 is not a power of 2.
  // The formula (x & (x-1)) == 0 is only valid for x > 0.
  // Powers of 2: 1, 2, 4, 8, 16, ... (2^n where n >= 0)
  // Zero is not in this sequence.
  // Expected: false for all types
  // Actual: true (BUG!)
  EXPECT_FALSE(is_pow2(static_cast<std::uint8_t>(0)))
      << "0 is not a power of 2, but is_pow2(0) returns true";
  EXPECT_FALSE(is_pow2(static_cast<std::uint16_t>(0)))
      << "0 is not a power of 2, but is_pow2(0) returns true";
  EXPECT_FALSE(is_pow2(static_cast<std::uint32_t>(0)))
      << "0 is not a power of 2, but is_pow2(0) returns true";
  EXPECT_FALSE(is_pow2(static_cast<std::uint64_t>(0)))
      << "0 is not a power of 2, but is_pow2(0) returns true";
}

TEST(IsPow2Test, OneShouldBeTrue) {
  // 2^0 = 1, the smallest power of 2
  // (1 & 0) = 0, so this should return true
  EXPECT_TRUE(is_pow2(static_cast<std::uint8_t>(1)));
  EXPECT_TRUE(is_pow2(static_cast<std::uint16_t>(1)));
  EXPECT_TRUE(is_pow2(static_cast<std::uint32_t>(1)));
  EXPECT_TRUE(is_pow2(static_cast<std::uint64_t>(1)));
}

TEST(IsPow2Test, AllPowersOfTwoUint8) {
  // Exhaustively test all powers of 2 for uint8_t
  for (int i = 0; i < 8; ++i) {
    std::uint8_t pow2_val = static_cast<std::uint8_t>(1u << i);
    EXPECT_TRUE(is_pow2(pow2_val))
        << "2^" << i << " = " << static_cast<int>(pow2_val)
        << " should be power of 2";
  }
}

TEST(IsPow2Test, AllPowersOfTwoUint16) {
  // Exhaustively test all powers of 2 for uint16_t
  for (int i = 0; i < 16; ++i) {
    std::uint16_t pow2_val = static_cast<std::uint16_t>(1u << i);
    EXPECT_TRUE(is_pow2(pow2_val))
        << "2^" << i << " = " << pow2_val << " should be power of 2";
  }
}

TEST(IsPow2Test, AllPowersOfTwoUint32) {
  // Exhaustively test all powers of 2 for uint32_t
  for (int i = 0; i < 32; ++i) {
    std::uint32_t pow2_val = 1u << i;
    EXPECT_TRUE(is_pow2(pow2_val))
        << "2^" << i << " = " << pow2_val << " should be power of 2";
  }
}

TEST(IsPow2Test, AllPowersOfTwoUint64) {
  // Exhaustively test all powers of 2 for uint64_t
  for (int i = 0; i < 64; ++i) {
    std::uint64_t pow2_val = 1ull << i;
    EXPECT_TRUE(is_pow2(pow2_val))
        << "2^" << i << " = " << pow2_val << " should be power of 2";
  }
}

TEST(IsPow2Test, NonPowersOfTwoUint8) {
  // Test non-powers of 2 return false
  std::uint8_t non_powers[] = {3,  5,  6,  7,   9,   10, 15,
                               17, 30, 31, 127, 254, 255};
  for (std::uint8_t val : non_powers) {
    EXPECT_FALSE(is_pow2(val))
        << static_cast<int>(val) << " is not a power of 2";
  }
}

TEST(IsPow2Test, NonPowersOfTwoUint16) {
  // Test non-powers of 2 return false
  std::uint16_t non_powers[] = {3,  5,   6,   7,    15,    17,    31,
                                33, 255, 257, 1000, 10000, 32767, 65535};
  for (std::uint16_t val : non_powers) {
    EXPECT_FALSE(is_pow2(val)) << val << " is not a power of 2";
  }
}

TEST(IsPow2Test, NonPowersOfTwoUint32) {
  // Test non-powers of 2 return false
  std::uint32_t non_powers[] = {3,    5,     6,       7,           15,
                                17,   31,    33,      255,         257,
                                1000, 10000, 1000000, 2147483647u, 4294967295u};
  for (std::uint32_t val : non_powers) {
    EXPECT_FALSE(is_pow2(val)) << val << " is not a power of 2";
  }
}

TEST(IsPow2Test, MaxValuesAllTypes) {
  // Max values are never powers of 2 (they're all bits set)
  EXPECT_FALSE(is_pow2(std::numeric_limits<std::uint8_t>::max()));
  EXPECT_FALSE(is_pow2(std::numeric_limits<std::uint16_t>::max()));
  EXPECT_FALSE(is_pow2(std::numeric_limits<std::uint32_t>::max()));
  EXPECT_FALSE(is_pow2(std::numeric_limits<std::uint64_t>::max()));
}

TEST(IsPow2Test, OneBeforeMaxAllTypes) {
  // Max-1 is also not a power of 2
  EXPECT_FALSE(is_pow2(
      static_cast<std::uint8_t>(std::numeric_limits<std::uint8_t>::max() - 1)));
  EXPECT_FALSE(is_pow2(static_cast<std::uint16_t>(
      std::numeric_limits<std::uint16_t>::max() - 1)));
  EXPECT_FALSE(is_pow2(static_cast<std::uint32_t>(
      std::numeric_limits<std::uint32_t>::max() - 1)));
  EXPECT_FALSE(is_pow2(static_cast<std::uint64_t>(
      std::numeric_limits<std::uint64_t>::max() - 1)));
}

TEST(IsPow2Test, ConstexprEvaluation) {
  // Test that is_pow2 works at compile time
  constexpr bool test_zero = is_pow2(static_cast<std::uint8_t>(0));
  constexpr bool test_one = is_pow2(static_cast<std::uint8_t>(1));
  constexpr bool test_two = is_pow2(static_cast<std::uint8_t>(2));
  constexpr bool test_three = is_pow2(static_cast<std::uint8_t>(3));

  EXPECT_FALSE(test_zero);
  EXPECT_TRUE(test_one);
  EXPECT_TRUE(test_two);
  EXPECT_FALSE(test_three);
}

// ============================================================================
// TEST SUITE: round_pow2()
// Tests rounding up to the next power of 2
// These tests demonstrate the compilation failures and logic errors
// ============================================================================

TEST(RoundPow2Test, CompilationErrorInRoundPow2ForUint32) {
  // This test would trigger the compilation error:
  // Line 21: if constexpr (size(x) > 2)  // should be sizeof(x)
  // Line 23: if constexpr (sizef(x) > 4) // should be sizeof(x)
  //
  // For uint32_t, the code tries to call size(x) which is undefined.
  // This causes compilation to fail for round_pow2 with uint32_t.
  //
  // NOTE: This is a compile-time error, so we can't test it at runtime.
  // The test suite that attempts to call round_pow2 will fail to compile.

  // This is documented but cannot be tested here.
  GTEST_SKIP()
      << "round_pow2 has compilation errors: size(x) and sizef(x) undefined";
}

TEST(RoundPow2Test, CompilationErrorInRoundPow2ForUint64) {
  // For uint64_t, same issue:
  // Line 21: if constexpr (size(x) > 2)  // should be sizeof(x)
  // Line 23: if constexpr (sizef(x) > 4) // should be sizeof(x)
  //
  // The x |= x >> 16 and x |= x >> 32 shifts will never be filled in
  // because the compilation fails, preventing proper bit filling for 64-bit
  // values.

  GTEST_SKIP()
      << "round_pow2 has compilation errors: size(x) and sizef(x) undefined";
}

// ============================================================================
// TEST SUITE: next_pow2()
// Tests getting the next power of 2 after a given value
// These tests demonstrate the compilation failures and logic errors
// ============================================================================

TEST(NextPow2Test, CompilationErrorInNextPow2ForUint32) {
  // This test would trigger the compilation error:
  // Line 34: if constexpr (size(x) > 2)  // should be sizeof(x)
  // Line 36: if constexpr (sizef(x) > 4) // should be sizeof(x)
  //
  // For uint32_t, the code tries to call size(x) which is undefined.
  // This causes compilation to fail for next_pow2 with uint32_t.

  GTEST_SKIP()
      << "next_pow2 has compilation errors: size(x) and sizef(x) undefined";
}

TEST(NextPow2Test, CompilationErrorInNextPow2ForUint64) {
  // For uint64_t, same issue as round_pow2:
  // Line 34: if constexpr (size(x) > 2)  // should be sizeof(x)
  // Line 36: if constexpr (sizef(x) > 4) // should be sizeof(x)
  //
  // The bit filling for 64-bit values is incomplete due to missing shifts.

  GTEST_SKIP()
      << "next_pow2 has compilation errors: size(x) and sizef(x) undefined";
}

// ============================================================================
// ADDITIONAL TESTS FOR LOGIC ERRORS IN is_pow2
// These tests successfully expose bugs in the current code
// ============================================================================

TEST(IsPow2LogicBug, ZeroEdgeCase) {
  // This is THE critical bug in is_pow2:
  // The formula (x & (x-1)) == 0 works for all positive powers of 2,
  // but it ALSO returns true for x = 0 because:
  // 0 & (0-1) = 0 & 0xFFFF... = 0
  //
  // Mathematical fact: 0 is NOT a power of 2.
  // Powers of 2 are defined as 2^n where n >= 0, giving: 1, 2, 4, 8, ...
  // Zero doesn't fit this definition.
  //
  // The correct implementation should be:
  // return (x != 0) && (x & (x - 1)) == 0;

  // This test WILL FAIL with the current code:
  EXPECT_FALSE(is_pow2(std::uint64_t{0}))
      << "BUG: is_pow2(0) returns true, but 0 is not a power of 2. "
      << "The bit trick (x & (x-1)) == 0 is only valid for x > 0.";
}

TEST(IsPow2Verification, AllCorrect) {
  // For reference: verify that 1 and all actual powers of 2 are detected
  // correctly This shows the function works for all non-zero powers of 2
  std::uint64_t powers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
  for (std::uint64_t p : powers) {
    EXPECT_TRUE(is_pow2(p)) << p << " is a power of 2";
  }
}

TEST(IsPow2Verification, AllWrong) {
  // Verify that non-powers of 2 are correctly identified
  std::uint64_t non_powers[] = {3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15};
  for (std::uint64_t p : non_powers) {
    EXPECT_FALSE(is_pow2(p)) << p << " is not a power of 2";
  }
}

// ============================================================================
// ROOT CAUSE ANALYSIS OF COMPILATION ERRORS
// ============================================================================

// The bugs in round_pow2 and next_pow2:
//
// ISSUE 1: Line 21 in round_pow2
//   if constexpr (size(x) > 2)    // should be sizeof(x)
//
//   - 'size' is not defined (not a standard function)
//   - Likely typo for 'sizeof'
//   - The intent: "if the type is larger than 2 bytes, shift by 16"
//   - Effect: Compilation fails for uint32_t and uint64_t
//
// ISSUE 2: Line 23 in round_pow2
//   if constexpr (sizef(x) > 4)   // should be sizeof(x)
//
//   - 'sizef' is not defined
//   - Likely typo for 'sizeof'
//   - The intent: "if the type is larger than 4 bytes, shift by 32"
//   - Effect: Compilation fails for uint64_t
//
// ISSUE 3: Line 34 in next_pow2 (identical to issue 1)
//   if constexpr (size(x) > 2)    // should be sizeof(x)
//
// ISSUE 4: Line 36 in next_pow2 (identical to issue 2)
//   if constexpr (sizef(x) > 4)   // should be sizeof(x)
//
// IMPACT ON ALGORITHM:
//
// For uint16_t:
//   - sizeof(uint16_t) = 2
//   - Checks: sizeof > 1 (true), size > 2 (ERROR), sizef > 4 (ERROR)
//   - Before error: x |= x >> 1, x |= x >> 2, x |= x >> 4, x |= x >> 8
//   - This is CORRECT for 16-bit values (fills all 16 bits)
//   - Result: uint16_t compiles and works correctly (despite the typos!)
//
// For uint32_t:
//   - sizeof(uint32_t) = 4
//   - Checks: sizeof > 1 (true), size > 2 (ERROR)
//   - Before error: x |= x >> 1, x |= x >> 2, x |= x >> 4, x |= x >> 8
//   - MISSING: x |= x >> 16 (required for 32-bit)
//   - Result: Bit filling is incomplete. Values with high bits set won't
//            be filled properly, causing incorrect power-of-2 rounding.
//   - Example: round_pow2(0x8001) should give 0x10000 (65536)
//             but will likely give wrong result due to incomplete fill.
//
// For uint64_t:
//   - sizeof(uint64_t) = 8
//   - Checks: sizeof > 1 (true), size > 2 (ERROR)
//   - Before error: x |= x >> 1, x |= x >> 2, x |= x >> 4, x |= x >> 8
//   - MISSING: x |= x >> 16 AND x |= x >> 32 (both required for 64-bit)
//   - Result: Algorithm completely broken for 64-bit values.

} // namespace
} // namespace franklin
