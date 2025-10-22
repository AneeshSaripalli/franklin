#include "container/column.hpp"
#include "core/bf16.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace franklin {

// ============================================================================
// ADVERSARIAL TEST #1: Column addition with size 15 (misses last element)
// ============================================================================

TEST(ColumnAdversarial, Int32AdditionSize15TailHandling) {
  // A) TEST CODE
  column_vector<Int32DefaultPolicy> a(15);
  column_vector<Int32DefaultPolicy> b(15);

  // Initialize with known pattern
  for (size_t i = 0; i < 15; ++i) {
    a.data()[i] = static_cast<int32_t>(i * 10);
    b.data()[i] = static_cast<int32_t>(i * 5);
  }

  auto result = a + b;

  // Verify ALL 15 elements, especially element 14 (last one)
  for (size_t i = 0; i < 15; ++i) {
    int32_t expected = static_cast<int32_t>(i * 10 + i * 5);
    int32_t actual = result.data()[i];
    EXPECT_EQ(actual, expected) << "FAILURE at index " << i << ": expected "
                                << expected << " but got " << actual << ". "
                                << "vectorize() loop 'while (offset + step <= "
                                   "num_elements)' at line 573 "
                                << "processes only elements 0-7 (step=8), "
                                   "leaving elements 8-14 uninitialized!";
  }
}

// B) DIAGNOSIS
// vectorize() at line 560:
// - step = 8 (AVX2 processes 8 int32 at a time)
// - num_elements = result.data().size() = ?
//
// Constructor at line 186: column_vector(15) computes:
// - elements_per_cache_line = 64 / sizeof(int32_t) = 64 / 4 = 16
// - rounded_size = ((15 + 16 - 1) / 16) * 16 = (30 / 16) * 16 = 1 * 16 = 16
// So data_.size() = 16 (rounded up to 16).
//
// vectorize() loop at line 573: while (offset + step <= num_elements)
// - offset = 0, step = 8, num_elements = 16
// - Iteration 1: offset=0, processes elements 0-7, offset += 8 → offset = 8
// - Iteration 2: offset=8, step=8, num_elements=16, 8+8 <= 16 ✓, processes
// elements 8-15, offset = 16
// - Iteration 3: offset=16, 16+8 <= 16 ✗, loop exits
//
// So actually all 16 elements are processed! But we only requested 15.
// Element 15 is in the padding and shouldn't be accessed by user.
//
// The real issue: Constructor sets present_mask_ for only elements 0-14:
// Line 199-201: for (i = 15; i < 16; ++i) present_mask_.set(i, false)
//
// So element 15 has undefined data but is marked as not present. This is
// correct. But if user accesses data()[15], they get garbage. However,
// present(15) returns false.
//
// Let me reconsider: The test checks elements 0-14, which are all processed.
// Actually this should work correctly!

// C) FIX DIRECTION
// This test should pass. The rounding and loop logic are correct. Need
// different edge case.

// ============================================================================
// ADVERSARIAL TEST #2: Column addition with size 7 (under SIMD width)
// ============================================================================

TEST(ColumnAdversarial, Int32AdditionSize7UnderSIMDWidth) {
  // A) TEST CODE
  column_vector<Int32DefaultPolicy> a(7);
  column_vector<Int32DefaultPolicy> b(7);

  for (size_t i = 0; i < 7; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  auto result = a + b;

  // Check all 7 elements
  for (size_t i = 0; i < 7; ++i) {
    int32_t expected = static_cast<int32_t>(i + i * 2);
    int32_t actual = result.data()[i];
    EXPECT_EQ(actual, expected)
        << "FAILURE at index " << i << ": expected " << expected << " but got "
        << actual << ". "
        << "Size 7 is less than AVX2 width (8), SIMD loop never runs, all "
           "elements uninitialized!";
  }
}

// B) DIAGNOSIS
// Constructor: elements_per_cache_line = 16, rounded_size = ((7 + 15) / 16) *
// 16 = 0 * 16 = 0 Wait, that's wrong! Let me recalculate: (7 + 16 - 1) / 16 =
// 22 / 16 = 1 (integer division) rounded_size = 1 * 16 = 16
//
// vectorize(): num_elements = 16, step = 8
// Loop: while (0 + 8 <= 16) processes elements 0-7, offset = 8
//       while (8 + 8 <= 16) processes elements 8-15, offset = 16
//       while (16 + 8 <= 16) exits
//
// So elements 0-15 are processed, including the 7 requested elements. ✓
//
// Actually, the issue is different: The constructor creates rounded_size = 16,
// but the addition operator at line 650 computes:
// effective_size = min(data_.size(), other.data_.size()) = min(16, 16) = 16
// output(16) creates a column of size 16 (rounded to 16).
//
// Then vectorize processes all 16 elements, but elements 7-15 have undefined
// present_mask. Present_mask is copied at line 596, so it's correct.
//
// The real test: Are elements 0-6 correct? They should be.

// C) FIX DIRECTION
// This should pass too. The logic handles small sizes correctly through
// rounding.

// ============================================================================
// ADVERSARIAL TEST #3: Column addition with size 17 (one element over SIMD
// boundary)
// ============================================================================

TEST(ColumnAdversarial, Float32AdditionSize17OneOverSIMDBoundary) {
  // A) TEST CODE
  column_vector<Float32DefaultPolicy> a(17);
  column_vector<Float32DefaultPolicy> b(17);

  for (size_t i = 0; i < 17; ++i) {
    a.data()[i] = static_cast<float>(i) * 1.5f;
    b.data()[i] = static_cast<float>(i) * 2.5f;
  }

  auto result = a + b;

  // Check all 17 elements, especially element 16
  for (size_t i = 0; i < 17; ++i) {
    float expected =
        static_cast<float>(i) * 1.5f + static_cast<float>(i) * 2.5f;
    float actual = result.data()[i];
    EXPECT_FLOAT_EQ(actual, expected)
        << "FAILURE at index " << i << ": expected " << expected << " but got "
        << actual << ". "
        << "Size 17 needs 2 SIMD iterations + 1 tail element, but loop has no "
           "tail handling!";
  }
}

// B) DIAGNOSIS
// elements_per_cache_line = 64 / sizeof(float) = 64 / 4 = 16
// rounded_size = ((17 + 15) / 16) * 16 = (32 / 16) * 16 = 2 * 16 = 32
// num_elements = 32, step = 8
//
// Loop: while (0 + 8 <= 32) → processes 0-7, offset = 8
//       while (8 + 8 <= 32) → processes 8-15, offset = 16
//       while (16 + 8 <= 32) → processes 16-23, offset = 24
//       while (24 + 8 <= 32) → processes 24-31, offset = 32
//       while (32 + 8 <= 32) exits
//
// All 32 elements processed, including elements 0-16. ✓
//
// But wait, element 17-31 are padding and have present_mask = false.
// They're processed by SIMD but shouldn't be accessed by user.

// C) FIX DIRECTION
// Still no bug found. The rounding to cache-line boundaries ensures no tail
// handling needed.

// ============================================================================
// ADVERSARIAL TEST #4: BF16 operations with size 13 (prime)
// ============================================================================

TEST(ColumnAdversarial, BF16MultiplicationSize13Prime) {
  // A) TEST CODE
  column_vector<BF16DefaultPolicy> a(13);
  column_vector<BF16DefaultPolicy> b(13);

  for (size_t i = 0; i < 13; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i + 1));
    b.data()[i] = bf16::from_float(3.0f);
  }

  auto result = a * b;

  // Verify all 13 elements
  for (size_t i = 0; i < 13; ++i) {
    float expected = static_cast<float>(i + 1) * 3.0f;
    float actual = result.data()[i].to_float();
    EXPECT_NEAR(actual, expected, 0.1f)
        << "FAILURE at index " << i << ": expected " << expected << " but got "
        << actual << ". "
        << "BF16 pipeline processes 8 elements at a time, size 13 needs tail "
           "handling!";
  }
}

// B) DIAGNOSIS
// elements_per_cache_line = 64 / sizeof(bf16) = 64 / 2 = 32
// rounded_size = ((13 + 31) / 32) * 32 = (44 / 32) * 32 = 1 * 32 = 32
// num_elements = 32, step = 8 (BF16Pipeline processes 8 bf16 at a time)
//
// Loop: Processes elements 0-7, 8-15, 16-23, 24-31 (4 iterations)
// All 32 elements processed, so elements 0-12 are correct. ✓

// C) FIX DIRECTION
// The rounding strategy avoids tail handling entirely. This is by design!

// ============================================================================
// ADVERSARIAL TEST #5: Scalar operations with size 9 (not aligned)
// ============================================================================

TEST(ColumnAdversarial, Int32ScalarAddSize9NotAligned) {
  // A) TEST CODE
  column_vector<Int32DefaultPolicy> a(9);

  for (size_t i = 0; i < 9; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }

  auto result = a + 100;

  // Check all 9 elements
  for (size_t i = 0; i < 9; ++i) {
    int32_t expected = static_cast<int32_t>(i + 100);
    int32_t actual = result.data()[i];
    EXPECT_EQ(actual, expected)
        << "FAILURE at index " << i << ": expected " << expected << " but got "
        << actual << ". "
        << "vectorize_scalar() at line 514 has same tail handling issue!";
  }
}

// B) DIAGNOSIS
// rounded_size = 16, num_elements = 16, step = 8
// vectorize_scalar() at line 526: while (offset + step <= num_elements)
// Processes elements 0-7 and 8-15, so all 9 requested elements are processed. ✓

// C) FIX DIRECTION
// Still working correctly due to cache-line rounding!

// ============================================================================
// ADVERSARIAL TEST #6: Present mask with size 31 (misaligned)
// ============================================================================

TEST(ColumnAdversarial, PresentMaskSize31MisalignedBitmask) {
  // A) TEST CODE
  column_vector<Int32DefaultPolicy> a(31);
  column_vector<Int32DefaultPolicy> b(31);

  for (size_t i = 0; i < 31; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  // Mark some elements as missing
  a.present_mask().set(5, false);
  a.present_mask().set(15, false);
  a.present_mask().set(30, false); // Last element

  b.present_mask().set(10, false);
  b.present_mask().set(30, false); // Last element

  auto result = a + b;

  // Verify bitmask intersection
  for (size_t i = 0; i < 31; ++i) {
    bool expected = a.present(i) && b.present(i);
    bool actual = result.present(i);
    EXPECT_EQ(actual, expected)
        << "FAILURE at index " << i << ": bitmask mismatch. "
        << "bitset_and_avx2() at line 505 may have issues with size 31 "
           "bitmask!";
  }

  // Specific checks
  EXPECT_FALSE(result.present(5)) << "Index 5 missing in a";
  EXPECT_FALSE(result.present(10)) << "Index 10 missing in b";
  EXPECT_FALSE(result.present(15)) << "Index 15 missing in a";
  EXPECT_FALSE(result.present(30)) << "Index 30 missing in both";
  EXPECT_TRUE(result.present(0)) << "Index 0 present in both";
  EXPECT_TRUE(result.present(20)) << "Index 20 present in both";
}

// B) DIAGNOSIS
// present_mask_ is dynamic_bitset<BitsetPolicy> with size matching
// data_.size(). For size 31, rounded_size = 32. present_mask_ has size 32 bits.
// dynamic_bitset rounds to 8 blocks = 512 bits minimum.
//
// bitset_and_avx2() at line 505 calls: dst &= src
// This uses dynamic_bitset::operator&= which should handle size 32 correctly.
//
// If there's a bug in dynamic_bitset for non-pow2 sizes, it will manifest here.

// C) FIX DIRECTION
// This tests the integration between column_vector and dynamic_bitset.
// If dynamic_bitset has bugs (from previous tests), they'll appear here.

// ============================================================================
// ADVERSARIAL TEST #7: Scalar subtraction (non-commutative) with size 23
// ============================================================================

TEST(ColumnAdversarial, Float32ScalarSubtractSize23NonCommutative) {
  // A) TEST CODE
  column_vector<Float32DefaultPolicy> a(23);

  for (size_t i = 0; i < 23; ++i) {
    a.data()[i] = static_cast<float>(i * 2);
  }

  // Scalar - column (non-commutative, requires special handling)
  auto result = 100.0f - a;

  // Verify all 23 elements
  for (size_t i = 0; i < 23; ++i) {
    float expected = 100.0f - static_cast<float>(i * 2);
    float actual = result.data()[i];
    EXPECT_FLOAT_EQ(actual, expected)
        << "FAILURE at index " << i << ": expected " << expected << " but got "
        << actual << ". "
        << "Scalar-column subtraction at line 872 may have SIMD loop boundary "
           "issues!";
  }
}

// B) DIAGNOSIS
// operator- at line 872: Manually implements scalar - column using SIMD.
// rounded_size = 32 (for size 23, elements_per_cache_line = 16)
// Loop at line 918: while (offset + step <= num_elements)
// step = 8, num_elements = 32
// Processes elements 0-7, 8-15, 16-23, 24-31 (4 iterations). ✓

// C) FIX DIRECTION
// Should work correctly.

// ============================================================================
// ADVERSARIAL TEST #8: Constructor rounding inconsistency
// ============================================================================

TEST(ColumnAdversarial, ConstructorRoundingInconsistency) {
  // A) TEST CODE
  // Test sizes around cache-line boundaries for int32 (16 per line)
  std::vector<size_t> test_sizes = {1, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65};

  for (size_t requested_size : test_sizes) {
    column_vector<Int32DefaultPolicy> col(requested_size);

    // elements_per_cache_line = 16
    size_t expected_rounded = ((requested_size + 15) / 16) * 16;

    size_t actual_size = col.data().size();
    EXPECT_EQ(actual_size, expected_rounded)
        << "FAILURE for requested size " << requested_size
        << ": data_.size() = " << actual_size << " but expected "
        << expected_rounded << ". "
        << "Constructor rounding at line 192-194 has off-by-one error!";

    // Verify present_mask size matches
    size_t mask_size = col.present_mask().size();
    EXPECT_EQ(mask_size, actual_size)
        << "present_mask_.size() doesn't match data_.size()";

    // Verify only requested elements are marked present
    for (size_t i = 0; i < requested_size; ++i) {
      EXPECT_TRUE(col.present(i)) << "Element " << i << " should be present";
    }
    for (size_t i = requested_size; i < actual_size; ++i) {
      EXPECT_FALSE(col.present(i))
          << "Padding element " << i << " should NOT be present";
    }
  }
}

// B) DIAGNOSIS
// Constructor at line 191-194:
// constexpr std::size_t elements_per_cache_line = 64 / sizeof(value_type);
// std::size_t rounded_size =
//     ((size + elements_per_cache_line - 1) / elements_per_cache_line) *
//     elements_per_cache_line;
//
// For int32_t: elements_per_cache_line = 16
// For size = 1: rounded = ((1 + 15) / 16) * 16 = (16 / 16) * 16 = 1 * 16 = 16 ✓
// For size = 15: rounded = ((15 + 15) / 16) * 16 = (30 / 16) * 16 = 1 * 16 = 16
// ✓ For size = 16: rounded = ((16 + 15) / 16) * 16 = (31 / 16) * 16 = 1 * 16 =
// 16 ✓ For size = 17: rounded = ((17 + 15) / 16) * 16 = (32 / 16) * 16 = 2 * 16
// = 32 ✓
//
// This is correct!

// C) FIX DIRECTION
// No bug in rounding logic. It's correct.

// ============================================================================
// ADVERSARIAL TEST #9: vectorize_destructive with mismatched sizes
// ============================================================================

TEST(ColumnAdversarial, DestructiveOpsMismatchedSizes) {
  // Operations on columns with different sizes are NOT supported.
  // The implementation now asserts when sizes don't match.
  // This test verifies that the assertion fires correctly.

  column_vector<Int32DefaultPolicy> a(31); // Larger
  column_vector<Int32DefaultPolicy> b(13); // Smaller

  for (size_t i = 0; i < 31; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
  }
  for (size_t i = 0; i < 13; ++i) {
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  // a + std::move(b) should now assert due to size mismatch
  EXPECT_DEATH(
      { auto result = a + std::move(b); },
      "Assertion failed.*present_mask_\\.size\\(\\) == "
      "other\\.present_mask_\\.size\\(\\)");
}

// B) DIAGNOSIS
// vectorize_destructive at line 613:
// num_elements = min(mut.data().size(), snd.data().size())
//              = min(32, 16) = 16 (rounded sizes)
//
// Loop processes elements 0-7 and 8-15 (2 iterations).
// So elements 0-12 (the requested 13) are all processed. ✓
//
// But present_mask intersection happens at line 639, which should handle it
// correctly.

// C) FIX DIRECTION
// Should work correctly.

// ============================================================================
// ADVERSARIAL TEST #10: Large non-pow2 sizes stress test
// ============================================================================

TEST(ColumnAdversarial, LargeSize997Prime) {
  // A) TEST CODE
  const size_t size = 997; // Large prime number
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> b(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<float>(i) * 0.5f;
    b.data()[i] = static_cast<float>(i) * 1.5f;
  }

  auto result = a + b;

  // Verify ALL elements, especially near boundaries
  std::vector<size_t> check_indices = {0,   1,   7,   8,   15,  16,  31,
                                       32,  63,  64,  127, 128, 255, 256,
                                       511, 512, 990, 991, 995, 996};

  for (size_t i : check_indices) {
    float expected =
        static_cast<float>(i) * 0.5f + static_cast<float>(i) * 1.5f;
    float actual = result.data()[i];
    EXPECT_FLOAT_EQ(actual, expected)
        << "FAILURE at index " << i << " (out of " << size << "): "
        << "Large prime size exposes cumulative rounding errors in "
           "vectorize()!";
  }

  // Check the very last element (most likely to fail)
  size_t last = size - 1;
  float expected_last =
      static_cast<float>(last) * 0.5f + static_cast<float>(last) * 1.5f;
  float actual_last = result.data()[last];
  EXPECT_FLOAT_EQ(actual_last, expected_last)
      << "CRITICAL FAILURE: Last element (index " << last << ") is incorrect! "
      << "This proves vectorize() has tail handling bug for non-pow2 sizes.";
}

// B) DIAGNOSIS
// elements_per_cache_line = 16 (float32)
// rounded_size = ((997 + 15) / 16) * 16 = (1012 / 16) * 16 = 63 * 16 = 1008
//
// vectorize() processes 1008 elements in steps of 8:
// 1008 / 8 = 126 iterations, covering all elements 0-1007.
// Elements 0-996 (the requested 997) are all processed. ✓

// C) FIX DIRECTION
// Should work correctly due to rounding!

// ============================================================================
// ADVERSARIAL TEST #11: Pathological size 1 (minimum)
// ============================================================================

TEST(ColumnAdversarial, PathologicalSize1Single) {
  // A) TEST CODE
  column_vector<Int32DefaultPolicy> a(1);
  column_vector<Int32DefaultPolicy> b(1);

  a.data()[0] = 42;
  b.data()[0] = 58;

  auto result = a + b;

  EXPECT_EQ(result.data()[0], 100)
      << "FAILURE: Single element addition failed. "
      << "Size 1 rounds to 16, but SIMD loop should still process element 0!";
  EXPECT_TRUE(result.present(0));
}

// B) DIAGNOSIS
// rounded_size = 16, num_elements = 16, step = 8
// Loop: processes elements 0-7 and 8-15.
// Element 0 is processed correctly. ✓

// C) FIX DIRECTION
// Should work.

// ============================================================================
// ADVERSARIAL TEST #12: BF16 with size 31 (odd boundary for 16-byte loads)
// ============================================================================

TEST(ColumnAdversarial, BF16Size31LoadAlignment) {
  // A) TEST CODE
  column_vector<BF16DefaultPolicy> a(31);
  column_vector<BF16DefaultPolicy> b(31);

  for (size_t i = 0; i < 31; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i));
    b.data()[i] = bf16::from_float(static_cast<float>(i + 10));
  }

  auto result = a + b;

  // Check all elements
  for (size_t i = 0; i < 31; ++i) {
    float expected = static_cast<float>(i) + static_cast<float>(i + 10);
    float actual = result.data()[i].to_float();
    EXPECT_NEAR(actual, expected, 0.1f)
        << "FAILURE at index " << i
        << ": BF16Pipeline loads 16 bytes (__m128i) "
        << "but size 31 may cause misaligned loads on element boundaries!";
  }
}

// B) DIAGNOSIS
// elements_per_cache_line = 32 (bf16 is 2 bytes)
// rounded_size = ((31 + 31) / 32) * 32 = (62 / 32) * 32 = 1 * 32 = 32
//
// BF16Pipeline: loads 8 bf16 at a time (16 bytes via _mm_load_si128)
// Processes elements 0-7, 8-15, 16-23, 24-31 (4 iterations, 32 elements total).
// Element 31 is in the 24-31 batch, which loads bytes 48-63 (offset 24 * 2 =
// 48). This is aligned to 16-byte boundary (48 % 16 = 0). ✓

// C) FIX DIRECTION
// Alignment is guaranteed by aligned_allocator<64>. Should work.

// ============================================================================
// SUMMARY TEST: Verify consistency across all non-pow2 sizes
// ============================================================================

TEST(ColumnAdversarial, ConsistencyAcrossNonPow2Sizes) {
  // A) TEST CODE
  std::vector<size_t> test_sizes = {1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 23,
                                    29, 31, 37, 41, 47, 53, 59, 61, 67, 71};

  for (size_t size : test_sizes) {
    column_vector<Int32DefaultPolicy> a(size);
    column_vector<Int32DefaultPolicy> b(size);

    // Initialize
    for (size_t i = 0; i < size; ++i) {
      a.data()[i] = static_cast<int32_t>(i);
      b.data()[i] = static_cast<int32_t>(100 - i);
    }

    // Test addition
    auto result_add = a + b;
    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ(result_add.data()[i], 100)
          << "Addition failed for size " << size << " at index " << i;
    }

    // Test subtraction
    auto result_sub = a - b;
    for (size_t i = 0; i < size; ++i) {
      int32_t expected = static_cast<int32_t>(i - (100 - i));
      EXPECT_EQ(result_sub.data()[i], expected)
          << "Subtraction failed for size " << size << " at index " << i;
    }

    // Test multiplication
    auto result_mul = a * b;
    for (size_t i = 0; i < size; ++i) {
      int32_t expected = static_cast<int32_t>(i * (100 - i));
      EXPECT_EQ(result_mul.data()[i], expected)
          << "Multiplication failed for size " << size << " at index " << i;
    }

    // Test scalar operations
    auto result_scalar = a + 50;
    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ(result_scalar.data()[i], static_cast<int32_t>(i + 50))
          << "Scalar addition failed for size " << size << " at index " << i;
    }
  }
}

// B) DIAGNOSIS
// This comprehensive test validates that all operations work correctly
// for a range of non-power-of-2 prime sizes. If there are any boundary
// condition bugs in the vectorize() loops or constructor rounding, this
// will catch them.

// C) FIX DIRECTION
// All tests should pass given the current cache-line rounding strategy.

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
