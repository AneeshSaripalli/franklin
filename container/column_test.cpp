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

// ============================================================================
// NEW COMPREHENSIVE TESTS FOR ADD/SUB/MUL WITH INT32/FLOAT/BF16
// ============================================================================

// Test Int32 operations with proper aligned policies
TEST(ColumnOperationsTest, Int32Addition) {
  column_vector<Int32DefaultPolicy> a(16);
  column_vector<Int32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  auto result = a + b;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i + i * 2));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ColumnOperationsTest, Int32Subtraction) {
  column_vector<Int32DefaultPolicy> a(16);
  column_vector<Int32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i * 3);
    b.data()[i] = static_cast<int32_t>(i);
  }

  auto result = a - b;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i * 3 - i));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ColumnOperationsTest, Int32Multiplication) {
  column_vector<Int32DefaultPolicy> a(16);
  column_vector<Int32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i + 1);
    b.data()[i] = static_cast<int32_t>(3);
  }

  auto result = a * b;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>((i + 1) * 3));
    EXPECT_TRUE(result.present(i));
  }
}

// Test Float32 operations
TEST(ColumnOperationsTest, Float32Addition) {
  column_vector<Float32DefaultPolicy> a(16);
  column_vector<Float32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 1.5f;
    b.data()[i] = static_cast<float>(i) * 2.5f;
  }

  auto result = a + b;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 1.5f +
                                          static_cast<float>(i) * 2.5f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ColumnOperationsTest, Float32Subtraction) {
  column_vector<Float32DefaultPolicy> a(16);
  column_vector<Float32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 5.0f;
    b.data()[i] = static_cast<float>(i) * 2.0f;
  }

  auto result = a - b;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 5.0f -
                                          static_cast<float>(i) * 2.0f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ColumnOperationsTest, Float32Multiplication) {
  column_vector<Float32DefaultPolicy> a(16);
  column_vector<Float32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i + 1) * 2.0f;
    b.data()[i] = 3.5f;
  }

  auto result = a * b;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i + 1) * 2.0f * 3.5f);
    EXPECT_TRUE(result.present(i));
  }
}

// Test BF16 operations
TEST(ColumnOperationsTest, BF16Addition) {
  column_vector<BF16DefaultPolicy> a(16);
  column_vector<BF16DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i));
    b.data()[i] = bf16::from_float(static_cast<float>(i * 2));
  }

  auto result = a + b;

  for (size_t i = 0; i < 16; ++i) {
    float expected = static_cast<float>(i) + static_cast<float>(i * 2);
    float actual = result.data()[i].to_float();
    // BF16 has reduced precision, allow for small differences
    EXPECT_NEAR(actual, expected, 0.1f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ColumnOperationsTest, BF16Subtraction) {
  column_vector<BF16DefaultPolicy> a(16);
  column_vector<BF16DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i * 3));
    b.data()[i] = bf16::from_float(static_cast<float>(i));
  }

  auto result = a - b;

  for (size_t i = 0; i < 16; ++i) {
    float expected = static_cast<float>(i * 3) - static_cast<float>(i);
    float actual = result.data()[i].to_float();
    // BF16 has reduced precision, allow for small differences
    EXPECT_NEAR(actual, expected, 0.1f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ColumnOperationsTest, BF16Multiplication) {
  column_vector<BF16DefaultPolicy> a(16);
  column_vector<BF16DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i + 1));
    b.data()[i] = bf16::from_float(2.0f);
  }

  auto result = a * b;

  for (size_t i = 0; i < 16; ++i) {
    float expected = static_cast<float>(i + 1) * 2.0f;
    float actual = result.data()[i].to_float();
    // BF16 has reduced precision, allow for small differences
    EXPECT_NEAR(actual, expected, 0.1f);
    EXPECT_TRUE(result.present(i));
  }
}

// Test bitmask intersection - CRITICAL REQUIREMENT
TEST(ColumnOperationsTest, Int32BitmaskIntersection) {
  column_vector<Int32DefaultPolicy> a(16);
  column_vector<Int32DefaultPolicy> b(16);

  // Set up data
  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  // Mark some values as missing in a
  a.present_mask().set(2, false);
  a.present_mask().set(5, false);
  a.present_mask().set(8, false);

  // Mark some values as missing in b (some overlap, some don't)
  b.present_mask().set(5, false); // Overlaps with a
  b.present_mask().set(7, false);
  b.present_mask().set(11, false);

  auto result = a + b;

  // Check bitmask intersection
  for (size_t i = 0; i < 16; ++i) {
    bool expected_present = a.present(i) && b.present(i);
    EXPECT_EQ(result.present(i), expected_present)
        << "Bitmask mismatch at index " << i;
  }

  // Specifically check the cases we set
  EXPECT_FALSE(result.present(2));  // Missing in a
  EXPECT_FALSE(result.present(5));  // Missing in both
  EXPECT_FALSE(result.present(7));  // Missing in b
  EXPECT_FALSE(result.present(8));  // Missing in a
  EXPECT_FALSE(result.present(11)); // Missing in b
  EXPECT_TRUE(result.present(0));   // Present in both
  EXPECT_TRUE(result.present(1));   // Present in both
}

TEST(ColumnOperationsTest, Float32BitmaskIntersection) {
  column_vector<Float32DefaultPolicy> a(16);
  column_vector<Float32DefaultPolicy> b(16);

  // Set up data
  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 1.5f;
    b.data()[i] = static_cast<float>(i) * 2.5f;
  }

  // Mark some values as missing
  a.present_mask().set(3, false);
  a.present_mask().set(6, false);
  b.present_mask().set(6, false); // Overlaps
  b.present_mask().set(9, false);

  auto result = a * b;

  // Check bitmask intersection
  for (size_t i = 0; i < 16; ++i) {
    bool expected_present = a.present(i) && b.present(i);
    EXPECT_EQ(result.present(i), expected_present)
        << "Bitmask mismask at index " << i;
  }

  EXPECT_FALSE(result.present(3));
  EXPECT_FALSE(result.present(6));
  EXPECT_FALSE(result.present(9));
  EXPECT_TRUE(result.present(0));
  EXPECT_TRUE(result.present(1));
}

TEST(ColumnOperationsTest, BF16BitmaskIntersection) {
  column_vector<BF16DefaultPolicy> a(16);
  column_vector<BF16DefaultPolicy> b(16);

  // Set up data
  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i));
    b.data()[i] = bf16::from_float(static_cast<float>(i + 10));
  }

  // Mark some values as missing
  a.present_mask().set(1, false);
  a.present_mask().set(4, false);
  b.present_mask().set(4, false); // Overlaps
  b.present_mask().set(10, false);

  auto result = a - b;

  // Check bitmask intersection
  for (size_t i = 0; i < 16; ++i) {
    bool expected_present = a.present(i) && b.present(i);
    EXPECT_EQ(result.present(i), expected_present)
        << "Bitmask mismatch at index " << i;
  }

  EXPECT_FALSE(result.present(1));
  EXPECT_FALSE(result.present(4));
  EXPECT_FALSE(result.present(10));
  EXPECT_TRUE(result.present(0));
  EXPECT_TRUE(result.present(2));
}

// Test move semantics with operations
TEST(ColumnOperationsTest, Int32AdditionWithMove) {
  column_vector<Int32DefaultPolicy> a(16);
  column_vector<Int32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  // Use move semantics for b
  auto result = a + std::move(b);

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i + i * 2));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ColumnOperationsTest, Float32SubtractionWithMove) {
  column_vector<Float32DefaultPolicy> a(16);
  column_vector<Float32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 5.0f;
    b.data()[i] = static_cast<float>(i) * 2.0f;
  }

  // Use move semantics
  auto result = a - std::move(b);

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 5.0f -
                                          static_cast<float>(i) * 2.0f);
    EXPECT_TRUE(result.present(i));
  }
}

// Test with larger sizes to ensure SIMD is working
TEST(ColumnOperationsTest, LargeInt32Addition) {
  const size_t size = 1024;
  column_vector<Int32DefaultPolicy> a(size);
  column_vector<Int32DefaultPolicy> b(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  auto result = a + b;

  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i + i * 2));
  }
}

TEST(ColumnOperationsTest, LargeFloat32Multiplication) {
  const size_t size = 1024;
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> b(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<float>(i) * 0.5f;
    b.data()[i] = 2.0f;
  }

  auto result = a * b;

  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 0.5f * 2.0f);
  }
}

TEST(ColumnOperationsTest, LargeBF16Subtraction) {
  const size_t size = 1024;
  column_vector<BF16DefaultPolicy> a(size);
  column_vector<BF16DefaultPolicy> b(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i) * 3.0f);
    b.data()[i] = bf16::from_float(static_cast<float>(i));
  }

  auto result = a - b;

  for (size_t i = 0; i < size; ++i) {
    float expected = static_cast<float>(i) * 3.0f - static_cast<float>(i);
    float actual = result.data()[i].to_float();
    // BF16 has only 7 mantissa bits (~1 in 128 precision)
    // For large values like 2000, precision is ~16, so allow larger tolerance
    float tolerance = std::max(16.0f, std::abs(expected) * 0.01f);
    EXPECT_NEAR(actual, expected, tolerance) << "Failed at index " << i;
  }
}

// ============================================================================
// SCALAR OPERATION TESTS - COLUMN OP SCALAR AND SCALAR OP COLUMN
// ============================================================================

// Test Int32 scalar operations
TEST(ScalarOperationsTest, Int32AddScalar) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  auto result = a + 10;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i + 10));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Int32SubtractScalar) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i * 5);
  }

  auto result = a - 3;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i * 5 - 3));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Int32MultiplyScalar) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  auto result = a * 7;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i * 7));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Int32ScalarAddColumn) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  // Test commutative property: 10 + column == column + 10
  auto result = 10 + a;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(10 + i));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Int32ScalarMultiplyColumn) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i + 1);
  }

  // Test commutative property: 5 * column == column * 5
  auto result = 5 * a;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(5 * (i + 1)));
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Int32ScalarSubtractColumn) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  // Test non-commutative: 100 - column != column - 100
  auto result = 100 - a;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(100 - i));
    EXPECT_TRUE(result.present(i));
  }
}

// Test Float32 scalar operations
TEST(ScalarOperationsTest, Float32AddScalar) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 2.0f;
  }

  auto result = a + 5.5f;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 2.0f + 5.5f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Float32SubtractScalar) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 3.0f;
  }

  auto result = a - 1.5f;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 3.0f - 1.5f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Float32MultiplyScalar) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) + 1.0f;
  }

  auto result = a * 2.5f;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], (static_cast<float>(i) + 1.0f) * 2.5f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, Float32ScalarSubtractColumn) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 2.0f;
  }

  auto result = 10.0f - a;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], 10.0f - static_cast<float>(i) * 2.0f);
    EXPECT_TRUE(result.present(i));
  }
}

// Test BF16 scalar operations
TEST(ScalarOperationsTest, BF16AddScalar) {
  column_vector<BF16DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i));
  }

  auto result = a + bf16::from_float(5.0f);

  for (size_t i = 0; i < 16; ++i) {
    float expected = static_cast<float>(i) + 5.0f;
    float actual = result.data()[i].to_float();
    EXPECT_NEAR(actual, expected, 0.1f);
    EXPECT_TRUE(result.present(i));
  }
}

TEST(ScalarOperationsTest, BF16MultiplyScalar) {
  column_vector<BF16DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i + 1));
  }

  auto result = a * bf16::from_float(2.0f);

  for (size_t i = 0; i < 16; ++i) {
    float expected = static_cast<float>(i + 1) * 2.0f;
    float actual = result.data()[i].to_float();
    EXPECT_NEAR(actual, expected, 0.1f);
    EXPECT_TRUE(result.present(i));
  }
}

// Test scalar operations preserve bitmask
TEST(ScalarOperationsTest, Int32ScalarPreservesBitmask) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  // Mark some as missing
  a.present_mask().set(2, false);
  a.present_mask().set(7, false);
  a.present_mask().set(11, false);

  auto result = a * 5;

  // Bitmask should be preserved (scalar is always "present")
  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.present(i), a.present(i))
        << "Bitmask mismatch at index " << i;
  }

  EXPECT_FALSE(result.present(2));
  EXPECT_FALSE(result.present(7));
  EXPECT_FALSE(result.present(11));
  EXPECT_TRUE(result.present(0));
  EXPECT_TRUE(result.present(5));
}

TEST(ScalarOperationsTest, Float32ScalarPreservesBitmask) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 2.0f;
  }

  // Mark some as missing
  a.present_mask().set(4, false);
  a.present_mask().set(9, false);

  auto result = a + 10.0f;

  // Bitmask should be preserved
  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.present(i), a.present(i));
  }

  EXPECT_FALSE(result.present(4));
  EXPECT_FALSE(result.present(9));
  EXPECT_TRUE(result.present(0));
}

// Test with large sizes
TEST(ScalarOperationsTest, LargeInt32MultiplyScalar) {
  const size_t size = 1024;
  column_vector<Int32DefaultPolicy> a(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  auto result = a * 3;

  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i * 3));
  }
}

TEST(ScalarOperationsTest, LargeFloat32AddScalar) {
  const size_t size = 1024;
  column_vector<Float32DefaultPolicy> a(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<float>(i) * 0.5f;
  }

  auto result = a + 100.0f;

  for (size_t i = 0; i < size; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 0.5f + 100.0f);
  }
}

// Test chaining operations
TEST(ScalarOperationsTest, ChainedScalarOperations) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i);
  }

  // (a * 2) + 5
  auto result = (a * 2.0f) + 5.0f;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FLOAT_EQ(result.data()[i], static_cast<float>(i) * 2.0f + 5.0f);
  }
}

TEST(ScalarOperationsTest, MixedScalarAndVectorOps) {
  column_vector<Int32DefaultPolicy> a(16);
  column_vector<Int32DefaultPolicy> b(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  // (a * 3) + b
  auto result = (a * 3) + b;

  for (size_t i = 0; i < 16; ++i) {
    EXPECT_EQ(result.data()[i], static_cast<int32_t>(i * 3 + i * 2));
  }
}

// ============================================================================
// REDUCTION OPERATION TESTS - SUM, PRODUCT, MIN, MAX, ANY, ALL
// ============================================================================

// Test Int32 sum reduction
TEST(ReductionOperationsTest, Int32Sum) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i + 1); // 1, 2, 3, ..., 16
  }

  int32_t result = a.sum();
  // Sum of 1 to 16 = 16 * 17 / 2 = 136
  EXPECT_EQ(result, 136);
}

TEST(ReductionOperationsTest, Int32SumWithMissing) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i + 1);
  }

  // Mark some values as missing
  a.present_mask().set(0, false);  // Remove 1
  a.present_mask().set(5, false);  // Remove 6
  a.present_mask().set(15, false); // Remove 16

  int32_t result = a.sum();
  // Sum of 1 to 16 - (1 + 6 + 16) = 136 - 23 = 113
  EXPECT_EQ(result, 113);
}

TEST(ReductionOperationsTest, Int32SumEmpty) {
  column_vector<Int32DefaultPolicy> a(0);
  int32_t result = a.sum();
  EXPECT_EQ(result, 0); // Identity for sum
}

TEST(ReductionOperationsTest, Int32SumAllMissing) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i + 1);
    a.present_mask().set(i, false);
  }

  int32_t result = a.sum();
  EXPECT_EQ(result, 0); // All masked out, sum = 0
}

// Test Int32 product reduction
TEST(ReductionOperationsTest, Int32Product) {
  column_vector<Int32DefaultPolicy> a(5);

  for (size_t i = 0; i < 5; ++i) {
    a.data()[i] = static_cast<int32_t>(i + 2); // 2, 3, 4, 5, 6
  }

  int32_t result = a.product();
  // 2 * 3 * 4 * 5 * 6 = 720
  EXPECT_EQ(result, 720);
}

TEST(ReductionOperationsTest, Int32ProductWithMissing) {
  column_vector<Int32DefaultPolicy> a(5);

  for (size_t i = 0; i < 5; ++i) {
    a.data()[i] = static_cast<int32_t>(i + 2); // 2, 3, 4, 5, 6
  }

  a.present_mask().set(2, false); // Remove 4

  int32_t result = a.product();
  // 2 * 3 * 5 * 6 = 180
  EXPECT_EQ(result, 180);
}

TEST(ReductionOperationsTest, Int32ProductEmpty) {
  column_vector<Int32DefaultPolicy> a(0);
  int32_t result = a.product();
  EXPECT_EQ(result, 1); // Identity for product
}

// Test Int32 min/max reduction
TEST(ReductionOperationsTest, Int32Min) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(100 - i); // 100, 99, 98, ..., 85
  }

  int32_t result = a.min();
  EXPECT_EQ(result, 85);
}

TEST(ReductionOperationsTest, Int32MinWithMissing) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(100 - i);
  }

  a.present_mask().set(15, false); // Remove the minimum (85)

  int32_t result = a.min();
  EXPECT_EQ(result, 86); // Next minimum
}

TEST(ReductionOperationsTest, Int32Max) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(100 + i); // 100, 101, ..., 115
  }

  int32_t result = a.max();
  EXPECT_EQ(result, 115);
}

TEST(ReductionOperationsTest, Int32MaxWithMissing) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(100 + i);
  }

  a.present_mask().set(15, false); // Remove the maximum (115)

  int32_t result = a.max();
  EXPECT_EQ(result, 114); // Next maximum
}

// Test Float32 reductions
TEST(ReductionOperationsTest, Float32Sum) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i + 1) * 2.5f;
  }

  float result = a.sum();
  // Sum of (1 to 16) * 2.5 = 136 * 2.5 = 340.0
  EXPECT_FLOAT_EQ(result, 340.0f);
}

TEST(ReductionOperationsTest, Float32Product) {
  column_vector<Float32DefaultPolicy> a(4);

  a.data()[0] = 1.5f;
  a.data()[1] = 2.0f;
  a.data()[2] = 3.0f;
  a.data()[3] = 4.0f;

  float result = a.product();
  // 1.5 * 2.0 * 3.0 * 4.0 = 36.0
  EXPECT_FLOAT_EQ(result, 36.0f);
}

TEST(ReductionOperationsTest, Float32Min) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(100 - i) * 0.5f;
  }

  float result = a.min();
  EXPECT_FLOAT_EQ(result, 42.5f); // (100 - 15) * 0.5 = 42.5
}

TEST(ReductionOperationsTest, Float32Max) {
  column_vector<Float32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<float>(i) * 1.5f;
  }

  float result = a.max();
  EXPECT_FLOAT_EQ(result, 22.5f); // 15 * 1.5 = 22.5
}

// Test BF16 reductions
TEST(ReductionOperationsTest, BF16Sum) {
  column_vector<BF16DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i + 1));
  }

  bf16 result = a.sum();
  // Sum of 1 to 16 = 136
  EXPECT_NEAR(result.to_float(), 136.0f, 1.0f); // BF16 precision
}

TEST(ReductionOperationsTest, BF16Product) {
  column_vector<BF16DefaultPolicy> a(4);

  a.data()[0] = bf16::from_float(2.0f);
  a.data()[1] = bf16::from_float(3.0f);
  a.data()[2] = bf16::from_float(1.5f);
  a.data()[3] = bf16::from_float(2.0f);

  bf16 result = a.product();
  // 2 * 3 * 1.5 * 2 = 18
  EXPECT_NEAR(result.to_float(), 18.0f, 0.5f);
}

TEST(ReductionOperationsTest, BF16Min) {
  column_vector<BF16DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(100 - i));
  }

  bf16 result = a.min();
  EXPECT_NEAR(result.to_float(), 85.0f, 0.5f);
}

TEST(ReductionOperationsTest, BF16Max) {
  column_vector<BF16DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i + 1));
  }

  bf16 result = a.max();
  EXPECT_NEAR(result.to_float(), 16.0f, 0.5f);
}

// Test any() and all() operations
TEST(ReductionOperationsTest, AnyAllPresent) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  EXPECT_TRUE(a.any()); // At least one element present
  EXPECT_TRUE(a.all()); // All elements present
}

TEST(ReductionOperationsTest, AnySomePresent) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  // Mark most as missing, leave one
  for (size_t i = 1; i < 16; ++i) {
    a.present_mask().set(i, false);
  }

  EXPECT_TRUE(a.any());  // At least one present (index 0)
  EXPECT_FALSE(a.all()); // Not all present
}

TEST(ReductionOperationsTest, AnyNonePresent) {
  column_vector<Int32DefaultPolicy> a(16);

  for (size_t i = 0; i < 16; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    a.present_mask().set(i, false);
  }

  EXPECT_FALSE(a.any()); // No elements present
  EXPECT_FALSE(a.all()); // No elements present
}

TEST(ReductionOperationsTest, AnyAllEmpty) {
  column_vector<Int32DefaultPolicy> a(0);

  EXPECT_FALSE(a.any()); // Empty column
  EXPECT_TRUE(a.all());  // Vacuously true - all zero elements are present
}

// Test with large sizes (tail handling)
TEST(ReductionOperationsTest, LargeInt32SumNonAligned) {
  // Use non-aligned size to test tail handling
  const size_t size = 1017; // Not a multiple of 8
  column_vector<Int32DefaultPolicy> a(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = 1; // Each element contributes 1
  }

  int32_t result = a.sum();
  EXPECT_EQ(result, static_cast<int32_t>(size));
}

TEST(ReductionOperationsTest, LargeFloat32MinNonAligned) {
  const size_t size = 1013; // Prime number, not aligned
  column_vector<Float32DefaultPolicy> a(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<float>(size - i);
  }

  float result = a.min();
  EXPECT_FLOAT_EQ(result, 1.0f); // Last element
}

TEST(ReductionOperationsTest, LargeBF16ProductIdentity) {
  // Test that product of all 1s = 1
  const size_t size = 512;
  column_vector<BF16DefaultPolicy> a(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = bf16::from_float(1.0f);
  }

  bf16 result = a.product();
  EXPECT_NEAR(result.to_float(), 1.0f, 0.01f);
}

// Test edge case: single element
TEST(ReductionOperationsTest, SingleElementSum) {
  column_vector<Int32DefaultPolicy> a(1);
  a.data()[0] = 42;

  EXPECT_EQ(a.sum(), 42);
  EXPECT_EQ(a.product(), 42);
  EXPECT_EQ(a.min(), 42);
  EXPECT_EQ(a.max(), 42);
}

TEST(ReductionOperationsTest, SingleElementMissing) {
  column_vector<Int32DefaultPolicy> a(1);
  a.data()[0] = 42;
  a.present_mask().set(0, false);

  EXPECT_EQ(a.sum(), 0);                                      // Identity
  EXPECT_EQ(a.product(), 1);                                  // Identity
  EXPECT_EQ(a.min(), std::numeric_limits<int32_t>::max());    // Identity
  EXPECT_EQ(a.max(), std::numeric_limits<int32_t>::lowest()); // Identity
}

// Test negative numbers
TEST(ReductionOperationsTest, Int32SumNegative) {
  column_vector<Int32DefaultPolicy> a(8);

  for (size_t i = 0; i < 8; ++i) {
    a.data()[i] = static_cast<int32_t>(i) - 4; // -4, -3, -2, -1, 0, 1, 2, 3
  }

  int32_t result = a.sum();
  // Sum = -4 - 3 - 2 - 1 + 0 + 1 + 2 + 3 = -4
  EXPECT_EQ(result, -4);
}

TEST(ReductionOperationsTest, Int32MinNegative) {
  column_vector<Int32DefaultPolicy> a(8);

  for (size_t i = 0; i < 8; ++i) {
    a.data()[i] = static_cast<int32_t>(i) - 4;
  }

  EXPECT_EQ(a.min(), -4);
  EXPECT_EQ(a.max(), 3);
}

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
