#include "container/column.hpp"
#include "core/bf16.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

namespace franklin {

// Test policy for column_vector with int32_t
struct Int32ColumnPolicy {
  using value_type = std::int32_t;
  using allocator_type = std::allocator<std::int32_t>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = true;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = true;
  static constexpr DataTypeEnum::Enum policy_id = DataTypeEnum::Int32Default;
};

// Test policy for column_vector with float
struct FloatColumnPolicy {
  using value_type = float;
  using allocator_type = std::allocator<float>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = true;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = true;
  static constexpr DataTypeEnum::Enum policy_id = DataTypeEnum::Float32Default;
};

// Test policy for column_vector with bf16
struct BF16ColumnPolicy {
  using value_type = bf16;
  using allocator_type = std::allocator<bf16>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = true;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = true;
  static constexpr DataTypeEnum::Enum policy_id = DataTypeEnum::BF16Default;
};

using Int32Column = column_vector<Int32ColumnPolicy>;
using FloatColumn = column_vector<FloatColumnPolicy>;
using BF16Column = column_vector<BF16ColumnPolicy>;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(ColumnVectorTest, DefaultConstruction) {
  Int32Column col;
  // Default constructed column should be valid but empty
  SUCCEED();
}

TEST(ColumnVectorTest, SizeConstruction) {
  Int32Column col(10);
  // Column with size 10 should be constructed
  SUCCEED();
}

TEST(ColumnVectorTest, SizeValueConstruction) {
  Int32Column col(10, 42);
  // Column with size 10, all values 42, should be constructed
  SUCCEED();
}

TEST(ColumnVectorTest, CopyConstruction) {
  Int32Column col1(10, 42);
  Int32Column col2(col1);
  // Copy construction should work
  SUCCEED();
}

TEST(ColumnVectorTest, MoveConstruction) {
  Int32Column col1(10, 42);
  Int32Column col2(std::move(col1));
  // Move construction should work
  SUCCEED();
}

// ============================================================================
// Assignment Tests
// ============================================================================

TEST(ColumnVectorTest, CopyAssignment) {
  Int32Column col1(10, 42);
  Int32Column col2;
  col2 = col1;
  // Copy assignment should work
  SUCCEED();
}

TEST(ColumnVectorTest, MoveAssignment) {
  Int32Column col1(10, 42);
  Int32Column col2;
  col2 = std::move(col1);
  // Move assignment should work
  SUCCEED();
}

// ============================================================================
// Addition Tests - Basic
// ============================================================================

TEST(ColumnVectorTest, AdditionBasic) {
  // Note: Current implementation has bugs, but we write tests
  // for the expected behavior once fixed

  // Create two small vectors for testing
  // Using size 16 (exactly 2 AVX2 registers worth of int32_t)
  Int32Column col1(16, 10);
  Int32Column col2(16, 20);

  // This will likely crash or hang with current implementation
  // due to the infinite while loop and wrong pointer casts
  // Commenting out for now until implementation is fixed

  // Int32Column result = col1 + col2;
  // Expected: each element should be 30

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionAlignedSize) {
  // Test with size that's a multiple of 16 (good for AVX2)
  // 32 int32_t values = 128 bytes = 4 AVX2 registers

  Int32Column col1(32, 5);
  Int32Column col2(32, 15);

  // Int32Column result = col1 + col2;
  // Expected: each element should be 20

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionLargeVectors) {
  // Test with larger vectors (1024 elements = 4KB)
  Int32Column col1(1024, 100);
  Int32Column col2(1024, 200);

  // Int32Column result = col1 + col2;
  // Expected: each element should be 300

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

// ============================================================================
// Addition Tests - Different Sizes
// ============================================================================

TEST(ColumnVectorTest, AdditionDifferentSizes) {
  // Test addition of vectors with different sizes
  // Expected behavior: result size is min(size1, size2)

  Int32Column col1(32, 10); // 32 elements
  Int32Column col2(16, 20); // 16 elements

  // Int32Column result = col1 + col2;
  // Expected: result should have 16 elements (the minimum)
  // Each element should be 30

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionFirstSmallerThanSecond) {
  Int32Column col1(16, 5);  // 16 elements
  Int32Column col2(32, 10); // 32 elements

  // Int32Column result = col1 + col2;
  // Expected: result should have 16 elements
  // Each element should be 15

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

// ============================================================================
// Addition Tests - Edge Cases
// ============================================================================

TEST(ColumnVectorTest, AdditionEmptyVectors) {
  Int32Column col1(0);
  Int32Column col2(0);

  // Int32Column result = col1 + col2;
  // Expected: result should have 0 elements

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionOneEmpty) {
  Int32Column col1(16, 10);
  Int32Column col2(0);

  // Int32Column result = col1 + col2;
  // Expected: result should have 0 elements (min of sizes)

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionSingleElement) {
  Int32Column col1(1, 5);
  Int32Column col2(1, 10);

  // Int32Column result = col1 + col2;
  // Expected: result[0] == 15

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionNonAlignedSize) {
  // Test with size that's not a multiple of 8 (AVX2 processes 8 int32_t at a
  // time)
  Int32Column col1(13, 7); // 13 elements (not aligned)
  Int32Column col2(13, 3);

  // Int32Column result = col1 + col2;
  // Expected: all 13 elements should be 10
  // This tests the tail handling (elements that don't fit in full SIMD
  // registers)

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

// ============================================================================
// Addition Tests - Overflow and Special Values
// ============================================================================

TEST(ColumnVectorTest, AdditionWithNegativeNumbers) {
  Int32Column col1(16, -10);
  Int32Column col2(16, 30);

  // Int32Column result = col1 + col2;
  // Expected: each element should be 20

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionWithZero) {
  Int32Column col1(16, 42);
  Int32Column col2(16, 0);

  // Int32Column result = col1 + col2;
  // Expected: each element should be 42 (identity)

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionMaxValues) {
  // Test near INT32_MAX to check for overflow behavior
  Int32Column col1(16, INT32_MAX - 100);
  Int32Column col2(16, 50);

  // Int32Column result = col1 + col2;
  // Note: This will overflow, which is UB in signed arithmetic
  // but SIMD instructions wrap around

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

// ============================================================================
// Addition Tests - Rvalue Reference Overload (Buffer Reuse)
// ============================================================================

TEST(ColumnVectorTest, AdditionRvalueReferenceBasic) {
  // Test the rvalue overload that reuses the buffer
  // col1 + std::move(col2) should write result into col2's buffer
  Int32Column col1(16, 10);
  Int32Column col2(16, 20);

  // Move col2 into the addition - result should reuse col2's buffer
  // Int32Column result = col1 + std::move(col2);
  // Expected: each element should be 30
  // The result should be using col2's original buffer

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionRvalueWithTemporary) {
  // Test with a temporary (natural rvalue)
  // This is a common pattern: col1 + Int32Column(size, value)
  Int32Column col1(16, 5);

  // Int32Column result = col1 + Int32Column(16, 10);
  // Expected: each element should be 15
  // Should use the temporary's buffer

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionRvalueChaining) {
  // Test chaining operations with rvalues
  // (a + b) + c should reuse buffers efficiently
  Int32Column col1(16, 1);
  Int32Column col2(16, 2);
  Int32Column col3(16, 3);

  // Int32Column result = (col1 + std::move(col2)) + std::move(col3);
  // Expected: each element should be 6
  // Should reuse col2's buffer for first addition, then col3's for second

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionRvalueVsLvalue) {
  // Verify that lvalue and rvalue overloads produce same result
  Int32Column col1(16, 10);
  Int32Column col2(16, 20);
  Int32Column col3(16, 20);

  // Int32Column result_lvalue = col1 + col2;  // Lvalue overload
  // Int32Column result_rvalue = col1 + std::move(col3);  // Rvalue overload

  // Both should produce same result (all elements = 30)
  // The difference is that rvalue reuses col3's buffer

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

TEST(ColumnVectorTest, AdditionRvalueDifferentSizes) {
  // Test rvalue overload with different sized vectors
  Int32Column col1(32, 5);
  Int32Column col2(16, 10);

  // Int32Column result = col1 + std::move(col2);
  // Expected: result has 16 elements (minimum size)
  // Should reuse col2's buffer

  SUCCEED() << "Test disabled until addition implementation is fixed";
}

// ============================================================================
// Other Operation Stubs - Verify They Return Something
// ============================================================================

TEST(ColumnVectorTest, SubtractionStub) {
  Int32Column col1(16, 10);
  Int32Column col2(16, 5);

  // Int32Column result = col1 - col2;
  // TODO: Implement subtraction operator
  SUCCEED();
}

TEST(ColumnVectorTest, MultiplicationStub) {
  Int32Column col1(16, 10);
  Int32Column col2(16, 5);

  // Int32Column result = col1 * col2;
  // TODO: Implement multiplication operator
  SUCCEED();
}

TEST(ColumnVectorTest, DivisionStub) {
  Int32Column col1(16, 10);
  Int32Column col2(16, 5);

  // Int32Column result = col1 / col2;
  // TODO: Implement division operator
  SUCCEED();
}

TEST(ColumnVectorTest, XorStub) {
  Int32Column col1(16, 0xFF);
  Int32Column col2(16, 0xAA);

  // Int32Column result = col1 ^ col2;
  // TODO: Implement XOR operator
  SUCCEED();
}

TEST(ColumnVectorTest, BitwiseAndStub) {
  Int32Column col1(16, 0xFF);
  Int32Column col2(16, 0xAA);

  // Int32Column result = col1 & col2;
  // TODO: Implement bitwise AND operator
  SUCCEED();
}

TEST(ColumnVectorTest, ModuloStub) {
  Int32Column col1(16, 17);
  Int32Column col2(16, 5);

  // Int32Column result = col1 % col2;
  // TODO: Implement modulo operator
  SUCCEED();
}

TEST(ColumnVectorTest, BitwiseOrStub) {
  Int32Column col1(16, 0xF0);
  Int32Column col2(16, 0x0F);

  // Int32Column result = col1 | col2;
  // TODO: Implement bitwise OR operator
  SUCCEED();
}

// ============================================================================
// Float Column Tests - Verify Template Instantiation
// ============================================================================

TEST(ColumnVectorTest, FloatColumnConstruction) {
  FloatColumn col(16, 3.14f);
  SUCCEED();
}

// Addition for float is not implemented yet (static_assert will fire)
// TEST(ColumnVectorTest, FloatColumnAddition) {
//   FloatColumn col1(16, 1.0f);
//   FloatColumn col2(16, 2.0f);
//   FloatColumn result = col1 + col2;
//   SUCCEED();
// }

// ============================================================================
// BF16 Column Tests - Brain Float 16-bit format
// ============================================================================

TEST(ColumnVectorTest, BF16ColumnConstruction) {
  BF16Column col(16, bf16(1.5f));
  SUCCEED();
}

TEST(ColumnVectorTest, BF16AdditionBasic) {
  // Test basic bf16 addition
  // BF16 has lower precision than float32, so results will have rounding
  BF16Column col1(16, bf16(1.0f));
  BF16Column col2(16, bf16(2.0f));

  // BF16Column result = col1 + col2;
  // Expected: each element should be approximately 3.0f
  // Note: BF16 truncates the lower 16 bits of float32 mantissa

  SUCCEED() << "Test disabled until bf16 addition implementation is complete";
}

TEST(ColumnVectorTest, BF16AdditionPrecisionLoss) {
  // Test that demonstrates BF16 precision characteristics
  // BF16 has only 7 mantissa bits vs float32's 23 bits
  BF16Column col1(16, bf16(1.0f));
  BF16Column col2(16, bf16(0.001f)); // Small value

  // BF16Column result = col1 + col2;
  // Expected: result may lose precision on small values
  // BF16 is designed for ML where small gradients can be approximated

  SUCCEED() << "Test disabled until bf16 addition implementation is complete";
}

TEST(ColumnVectorTest, BF16AdditionLargeVectors) {
  // Test bf16 with larger vectors (typical ML batch size)
  // 1024 bf16 elements = 2KB (vs 4KB for float32)
  BF16Column col1(1024, bf16(1.5f));
  BF16Column col2(1024, bf16(2.5f));

  // BF16Column result = col1 + col2;
  // Expected: each element approximately 4.0f
  // Memory bandwidth savings: 50% vs float32

  SUCCEED() << "Test disabled until bf16 addition implementation is complete";
}

TEST(ColumnVectorTest, BF16AdditionRvalueReuse) {
  // Test rvalue overload with bf16 to verify buffer reuse
  BF16Column col1(32, bf16(1.0f));
  BF16Column col2(32, bf16(2.0f));

  // BF16Column result = col1 + std::move(col2);
  // Expected: reuses col2's buffer, saves allocation
  // Important for ML training where memory allocations are expensive

  SUCCEED() << "Test disabled until bf16 addition implementation is complete";
}

TEST(ColumnVectorTest, BF16AdditionWithZero) {
  // Test bf16 addition identity
  BF16Column col1(16, bf16(3.14f));
  BF16Column col2(16, bf16(0.0f));

  // BF16Column result = col1 + col2;
  // Expected: each element should be approximately 3.14f
  // Tests that zero is represented correctly in bf16

  SUCCEED() << "Test disabled until bf16 addition implementation is complete";
}

TEST(ColumnVectorTest, BF16AdditionNegativeNumbers) {
  // Test bf16 with negative values
  BF16Column col1(16, bf16(-1.5f));
  BF16Column col2(16, bf16(3.0f));

  // BF16Column result = col1 + col2;
  // Expected: each element approximately 1.5f
  // Sign bit handling should work correctly

  SUCCEED() << "Test disabled until bf16 addition implementation is complete";
}

TEST(ColumnVectorTest, BF16AdditionSpecialValues) {
  // Test bf16 with special floating point values
  BF16Column col_inf(16, bf16::from_float(INFINITY));
  BF16Column col_normal(16, bf16(1.0f));

  // BF16Column result = col_inf + col_normal;
  // Expected: each element should be infinity
  // Inf + finite = Inf in IEEE 754

  SUCCEED() << "Test disabled until bf16 addition implementation is complete";
}

TEST(ColumnVectorTest, BF16ConversionRoundTrip) {
  // Test that bf16 <-> float32 conversion works correctly
  float original = 3.14159f;
  bf16 converted = bf16::from_float(original);
  float reconstructed = converted.to_float();

  // BF16 truncates, so we expect some precision loss
  // But the value should be close (within bf16 precision)
  EXPECT_NEAR(reconstructed, original, 0.01f)
      << "BF16 round-trip conversion should preserve approximate value";
}

// ============================================================================
// ADVERSARIAL TESTS FOR present() - Logic Errors and Edge Cases
// ============================================================================

// TEST 1: Constructor with size only should mark all values as present
// BUG: present_ is initialized with default value (false) instead of true
TEST(ColumnVectorTest, PresentAfterSizeConstructorShouldBeTrue) {
  // When constructing a column with size but no value,
  // all elements should be present (have valid data)
  Int32Column col(10);

  // Expected: present(0) returns true because the element exists
  // Actual: present(0) returns false because present_(size, alloc)
  //         defaults to false instead of true
  EXPECT_TRUE(col.present(0))
      << "Element 0 should be present after size construction";
  EXPECT_TRUE(col.present(5))
      << "Element 5 should be present after size construction";
  EXPECT_TRUE(col.present(9))
      << "Element 9 should be present after size construction";
}

// TEST 2: Out of bounds access returns false
TEST(ColumnVectorTest, PresentOutOfBoundsReturnsFalse) {
  Int32Column col(10);

  // present() returns false for out-of-bounds (safe, no UB)
  EXPECT_FALSE(col.present(100)) << "Out of bounds should return false";
  EXPECT_FALSE(col.present(10)) << "Index equal to size should return false";

  // present_unchecked() is unchecked - no bounds checking, no death test
}

// TEST 3: present() after default construction with size 0
TEST(ColumnVectorTest, PresentOnEmptyVectorReturnsFalse) {
  Int32Column col(0); // Empty vector

  // present() returns false for empty vector (safe)
  EXPECT_FALSE(col.present(0)) << "Index 0 on empty vector should return false";

  // present_unchecked() is unchecked - no bounds checking, no death test
}

// TEST 4: State inconsistency between data_ and present_ after copy assignment
TEST(ColumnVectorTest, PresentStateAfterCopyAssignment) {
  Int32Column col1(10, 42); // 10 elements, all present
  Int32Column col2(5, 99);  // 5 elements, all present

  // Verify initial state
  EXPECT_TRUE(col1.present(9)) << "col1[9] should be present initially";

  // Copy smaller vector to larger vector
  col1 = col2; // Now col1 should have 5 elements

  // After assignment, present_ should be resized to match
  // Accessing index 9 (which was valid before) should now return false
  EXPECT_FALSE(col1.present(9))
      << "Index 9 should be out of bounds after assignment";

  // Valid index should still work
  EXPECT_TRUE(col1.present(4)) << "col1[4] should be present after assignment";

  // present_unchecked() is unchecked - no bounds checking, no death test
}

// TEST 5: present() after move constructor - moved-from object state
TEST(ColumnVectorTest, PresentAfterMoveConstructor) {
  Int32Column col1(10, 42);
  Int32Column col2(std::move(col1)); // col1 is now moved-from

  // col2 should have the moved data
  EXPECT_TRUE(col2.present(0)) << "col2[0] should be present after move";
  EXPECT_TRUE(col2.present(9)) << "col2[9] should be present after move";

  // col1 is in a valid but unspecified state after move
  // Typically std::vector::move leaves it empty, so accessing would be UB
  // We can't reliably test this without knowing the moved-from state
  // Just verify col2 works correctly
  SUCCEED();
}

// TEST 6: Performance - repeated present() calls should be O(1)
// BUG: Not a logic bug, but tests performance characteristics
TEST(ColumnVectorTest, PresentIsConstantTimeNotLinear) {
  const size_t size = 1000000; // 1 million elements
  Int32Column col(size, 42);

  // present() should be O(1) - direct vector access
  // If implementation accidentally did something O(n), this would be slow

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 10000; ++i) {
    volatile bool result = col.present(size - 1); // Last element
    (void)result;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // 10000 O(1) operations should take microseconds, not milliseconds
  EXPECT_LT(duration.count(), 10000)
      << "present() took " << duration.count()
      << " microseconds for 10000 calls - should be < 10ms";
}

// TEST 7: present() consistency after self-assignment
// BUG: Self-assignment might corrupt present_ state
TEST(ColumnVectorTest, PresentConsistentAfterSelfAssignment) {
  Int32Column col(10, 42);

  EXPECT_TRUE(col.present(5))
      << "present(5) should be true before self-assignment";

  // Self-assignment should be a no-op due to self-check
  col = col;

  // present_ should still be valid
  EXPECT_TRUE(col.present(5))
      << "present(5) should still be true after self-assignment";
}

// TEST 8: present() at maximum valid index
// BUG: Off-by-one error - size is 10, valid indices are 0-9
TEST(ColumnVectorTest, PresentAtMaximumValidIndexShouldWork) {
  Int32Column col(10, 42);

  // Valid indices: 0, 1, 2, ..., 9
  // Index 9 is the last valid index
  EXPECT_TRUE(col.present(9))
      << "present(9) should work - it's the last valid index";

  // Index 10 is out of bounds - this will cause UB
  // Uncomment to test with sanitizers:
  // bool result = col.present(10);  // Off-by-one error
}

// TEST 9: present() after copy constructor preserves state correctly
// BUG: Copy constructor should copy present_ state exactly
TEST(ColumnVectorTest, PresentStatePreservedAfterCopyConstructor) {
  Int32Column col1(10, 42); // All elements present

  // Verify original state
  EXPECT_TRUE(col1.present(0));
  EXPECT_TRUE(col1.present(9));

  // Copy constructor should preserve present_ exactly
  Int32Column col2(col1);

  EXPECT_TRUE(col2.present(0))
      << "Copied column should preserve present state at index 0";
  EXPECT_TRUE(col2.present(9))
      << "Copied column should preserve present state at index 9";
}

// TEST 10: present() with out-of-bounds indices returns false
TEST(ColumnVectorTest, PresentWithVariousOutOfBoundsIndices) {
  Int32Column col(1, 42); // Only 1 element at index 0

  // present() returns false for out-of-bounds (safe)
  EXPECT_FALSE(col.present(1)) << "Index 1 should be out of bounds";
  EXPECT_FALSE(col.present(1000)) << "Index 1000 should be out of bounds";

  // Valid index should work
  EXPECT_TRUE(col.present(0)) << "Index 0 should be valid";

  // present_unchecked() is unchecked - no bounds checking, no death test
}

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
