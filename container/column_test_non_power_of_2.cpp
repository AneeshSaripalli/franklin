// Comprehensive test suite for non-power-of-2 array sizes in reduction
// operations This file tests the tail-handling logic critical for correctness
// when num_elements is not a multiple of 8.
//
// Key test scenarios:
// 1. Pure tail cases (sizes 1-7): No full vectors, only tail processing
// 2. Tail + one vector (sizes 9-15): Tests main loop → tail transition
// 3. Power-of-2 boundaries (256±1, 1024±1, 65536±1): Tests chunk boundaries
// 4. Mask pattern variations: Tests identity blending in tail
// 5. All four operations: sum, product, min, max
// 6. All three types: int32, float32, bf16

#include "container/column.hpp"
#include "core/bf16.hpp"
#include <cmath>
#include <gtest/gtest.h>

namespace franklin {

using Int32Column = column_vector<Int32DefaultPolicy>;
using FloatColumn = column_vector<Float32DefaultPolicy>;
using BF16Column = column_vector<BF16DefaultPolicy>;

// ============================================================================
// PURE TAIL TEST CASES (Sizes 1-7: No Full Vectors)
//
// These expose initialization and identity blending bugs immediately
// ============================================================================

// Size 2: Two elements (no full vectors, pure tail)
TEST(NonPowerOf2ReductionTest, Size2Int32Sum) {
  Int32Column col(2);
  col.data()[0] = 5;
  col.data()[1] = 7;
  // Expected: 5 + 7 = 12 (not 0, the identity)
  EXPECT_EQ(col.sum(), 12) << "Pure tail sum should include all elements";
}

TEST(NonPowerOf2ReductionTest, Size2Int32Product) {
  Int32Column col(2);
  col.data()[0] = 3;
  col.data()[1] = 4;
  // Expected: 3 * 4 = 12 (not 1, the identity)
  EXPECT_EQ(col.product(), 12)
      << "Pure tail product should multiply all elements";
}

TEST(NonPowerOf2ReductionTest, Size2Int32Min) {
  Int32Column col(2);
  col.data()[0] = 100;
  col.data()[1] = 50;
  // Expected: 50 (not INT_MAX, the identity)
  EXPECT_EQ(col.min(), 50) << "Pure tail min should find minimum";
}

TEST(NonPowerOf2ReductionTest, Size2Int32Max) {
  Int32Column col(2);
  col.data()[0] = 100;
  col.data()[1] = 50;
  // Expected: 100 (not INT_MIN, the identity)
  EXPECT_EQ(col.max(), 100) << "Pure tail max should find maximum";
}

// Size 3: Three elements (odd, forces specific tail processing)
TEST(NonPowerOf2ReductionTest, Size3Int32Sum) {
  Int32Column col(3);
  col.data()[0] = 1;
  col.data()[1] = 2;
  col.data()[2] = 3;
  // Expected: 1 + 2 + 3 = 6
  EXPECT_EQ(col.sum(), 6) << "Size 3 sum should be 6";
}

TEST(NonPowerOf2ReductionTest, Size3Int32Product) {
  Int32Column col(3);
  col.data()[0] = 2;
  col.data()[1] = 3;
  col.data()[2] = 4;
  // Expected: 2 * 3 * 4 = 24
  EXPECT_EQ(col.product(), 24) << "Size 3 product should be 24";
}

// Size 4: Four elements (half vector)
TEST(NonPowerOf2ReductionTest, Size4Int32Sum) {
  Int32Column col(4);
  for (int i = 0; i < 4; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, 4
  }
  // Expected: 1 + 2 + 3 + 4 = 10
  EXPECT_EQ(col.sum(), 10) << "Size 4 sum should be 10";
}

TEST(NonPowerOf2ReductionTest, Size4Int32Min) {
  Int32Column col(4);
  col.data()[0] = 100;
  col.data()[1] = 50;
  col.data()[2] = 75;
  col.data()[3] = 25;
  // Expected: 25
  EXPECT_EQ(col.min(), 25) << "Size 4 min should be 25";
}

// Size 5: Five elements (critical for tail logic)
TEST(NonPowerOf2ReductionTest, Size5Int32Sum) {
  Int32Column col(5);
  for (int i = 0; i < 5; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, 4, 5
  }
  // Expected: 1 + 2 + 3 + 4 + 5 = 15
  EXPECT_EQ(col.sum(), 15) << "Size 5 sum should be 15";
}

TEST(NonPowerOf2ReductionTest, Size5Int32Product) {
  Int32Column col(5);
  for (int i = 0; i < 5; ++i) {
    col.data()[i] = i + 2; // 2, 3, 4, 5, 6
  }
  // Expected: 2 * 3 * 4 * 5 * 6 = 720
  EXPECT_EQ(col.product(), 720) << "Size 5 product should be 720";
}

TEST(NonPowerOf2ReductionTest, Size5Int32Max) {
  Int32Column col(5);
  col.data()[0] = 10;
  col.data()[1] = 50;
  col.data()[2] = 30;
  col.data()[3] = 20;
  col.data()[4] = 40;
  // Expected: 50
  EXPECT_EQ(col.max(), 50) << "Size 5 max should be 50";
}

// Size 6: Six elements (tests mask boundary)
TEST(NonPowerOf2ReductionTest, Size6Int32Sum) {
  Int32Column col(6);
  for (int i = 0; i < 6; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, 4, 5, 6
  }
  // Expected: 1 + 2 + 3 + 4 + 5 + 6 = 21
  EXPECT_EQ(col.sum(), 21) << "Size 6 sum should be 21";
}

// Size 7: Seven elements (maximum pure tail before full vector)
TEST(NonPowerOf2ReductionTest, Size7Int32Sum) {
  Int32Column col(7);
  for (int i = 0; i < 7; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, 4, 5, 6, 7
  }
  // Expected: 1 + 2 + 3 + 4 + 5 + 6 + 7 = 28
  EXPECT_EQ(col.sum(), 28) << "Size 7 sum should be 28";
}

TEST(NonPowerOf2ReductionTest, Size7Int32Product) {
  Int32Column col(7);
  col.data()[0] = 1;
  col.data()[1] = 2;
  col.data()[2] = 2;
  col.data()[3] = 1;
  col.data()[4] = 3;
  col.data()[5] = 1;
  col.data()[6] = 2;
  // Expected: 1 * 2 * 2 * 1 * 3 * 1 * 2 = 24
  EXPECT_EQ(col.product(), 24) << "Size 7 product should be 24";
}

// ============================================================================
// TAIL MASK PATTERN TESTS (Pure Tail Cases with Masking)
//
// These test that identity blending works correctly in tail
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size5SumTailAllMasked) {
  Int32Column col(5);
  for (int i = 0; i < 5; ++i) {
    col.data()[i] = i + 1;            // 1, 2, 3, 4, 5
    col.present_mask().set(i, false); // All masked
  }
  // Expected: 0 (identity for sum, all masked)
  EXPECT_EQ(col.sum(), 0) << "All masked pure tail sum should return identity";
}

TEST(NonPowerOf2ReductionTest, Size5ProductTailAllMasked) {
  Int32Column col(5);
  for (int i = 0; i < 5; ++i) {
    col.data()[i] = i + 2;
    col.present_mask().set(i, false); // All masked
  }
  // Expected: 1 (identity for product, all masked)
  EXPECT_EQ(col.product(), 1)
      << "All masked pure tail product should return identity";
}

TEST(NonPowerOf2ReductionTest, Size4MinTailFirstElementMasked) {
  Int32Column col(4);
  col.data()[0] = 10;
  col.data()[1] = 50;
  col.data()[2] = 75;
  col.data()[3] = 25;
  col.present_mask().set(0, false); // First element masked
  // Expected: 25 (skip 10, find min of 50, 75, 25)
  EXPECT_EQ(col.min(), 25) << "Min with first element masked should skip it";
}

TEST(NonPowerOf2ReductionTest, Size4MaxTailLastElementMasked) {
  Int32Column col(4);
  col.data()[0] = 10;
  col.data()[1] = 50;
  col.data()[2] = 75;
  col.data()[3] = 100;
  col.present_mask().set(3, false); // Last element masked
  // Expected: 75 (skip 100, find max of 10, 50, 75)
  EXPECT_EQ(col.max(), 75) << "Max with last element masked should skip it";
}

// ============================================================================
// ONE VECTOR + TAIL TEST CASES (Sizes 9-15)
//
// These test main loop → tail transition and accumulator carryover
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size9Int32Sum) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, 4, 5, 6, 7, 8, 9
  }
  // Expected: 1 + 2 + ... + 9 = 45
  EXPECT_EQ(col.sum(), 45) << "Size 9 sum should be 45 (one vector + 1 tail)";
}

TEST(NonPowerOf2ReductionTest, Size9Int32Product) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = 2;
  }
  // Expected: 2^9 = 512
  EXPECT_EQ(col.product(), 512) << "Size 9 product should be 2^9 = 512";
}

TEST(NonPowerOf2ReductionTest, Size9Int32Min) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = 100 - i; // 100, 99, 98, ..., 92
  }
  // Expected: 92 (last element)
  EXPECT_EQ(col.min(), 92)
      << "Size 9 min should find minimum across main loop + tail";
}

TEST(NonPowerOf2ReductionTest, Size9Int32Max) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = 50 + i; // 50, 51, 52, ..., 58
  }
  // Expected: 58
  EXPECT_EQ(col.max(), 58) << "Size 9 max should be 58";
}

// Critical: Size 9 with single tail element
TEST(NonPowerOf2ReductionTest, Size9ProductWithTailMasked) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = 2;
  }
  col.present_mask().set(8, false); // Only tail element masked
  // Expected: 2^8 = 256 (skip the 9th element)
  EXPECT_EQ(col.product(), 256)
      << "Size 9 product with tail masked should be 2^8 = 256";
}

// Size 10: One vector + 2 tail
TEST(NonPowerOf2ReductionTest, Size10Int32Sum) {
  Int32Column col(10);
  for (int i = 0; i < 10; ++i) {
    col.data()[i] = i; // 0, 1, 2, ..., 9
  }
  // Expected: 0 + 1 + 2 + ... + 9 = 45
  EXPECT_EQ(col.sum(), 45) << "Size 10 sum should be 45";
}

// Size 15: One vector + 7 tail (critical boundary)
TEST(NonPowerOf2ReductionTest, Size15Int32Sum) {
  Int32Column col(15);
  for (int i = 0; i < 15; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 15 (all 1s)
  EXPECT_EQ(col.sum(), 15) << "Size 15 sum should be 15";
}

TEST(NonPowerOf2ReductionTest, Size15Int32Min) {
  Int32Column col(15);
  for (int i = 0; i < 15; ++i) {
    col.data()[i] = i; // 0, 1, 2, ..., 14
  }
  // Expected: 0 (minimum)
  EXPECT_EQ(col.min(), 0) << "Size 15 min should be 0";
}

// ============================================================================
// TWO VECTORS + TAIL TEST CASES (Sizes 17+)
//
// These test accumulator state across multiple full vectors
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size17Int32Sum) {
  Int32Column col(17);
  for (int i = 0; i < 17; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, ..., 17
  }
  // Expected: 1 + 2 + ... + 17 = 153
  EXPECT_EQ(col.sum(), 153) << "Size 17 sum should be 153";
}

TEST(NonPowerOf2ReductionTest, Size17Int32Min) {
  Int32Column col(17);
  for (int i = 0; i < 17; ++i) {
    col.data()[i] = 100 - i; // 100, 99, ..., 84
  }
  // Expected: 84 (last element)
  EXPECT_EQ(col.min(), 84) << "Size 17 min should be 84";
}

TEST(NonPowerOf2ReductionTest, Size31Int32Sum) {
  Int32Column col(31);
  for (int i = 0; i < 31; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 31
  EXPECT_EQ(col.sum(), 31) << "Size 31 sum should be 31";
}

// ============================================================================
// POWER-OF-2 BOUNDARY CASES (256±1, 1024±1)
//
// These test near-alignment boundaries and chunking edge cases
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size255Int32Sum) {
  Int32Column col(255);
  for (size_t i = 0; i < 255; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 255
  EXPECT_EQ(col.sum(), 255)
      << "Size 255 (31 vectors + 7 tail) sum should be 255";
}

TEST(NonPowerOf2ReductionTest, Size256Int32Sum) {
  Int32Column col(256);
  for (size_t i = 0; i < 256; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 256 (exactly 32 vectors)
  EXPECT_EQ(col.sum(), 256) << "Size 256 sum should be 256";
}

TEST(NonPowerOf2ReductionTest, Size257Int32Sum) {
  Int32Column col(257);
  for (size_t i = 0; i < 257; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 257 (32 vectors + 1 tail)
  EXPECT_EQ(col.sum(), 257) << "Size 257 sum should be 257";
}

TEST(NonPowerOf2ReductionTest, Size1023Int32Sum) {
  Int32Column col(1023);
  for (size_t i = 0; i < 1023; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 1023 (127 vectors + 7 tail)
  EXPECT_EQ(col.sum(), 1023) << "Size 1023 sum should be 1023";
}

TEST(NonPowerOf2ReductionTest, Size1024Int32Sum) {
  Int32Column col(1024);
  for (size_t i = 0; i < 1024; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 1024 (exactly 128 vectors)
  EXPECT_EQ(col.sum(), 1024) << "Size 1024 sum should be 1024";
}

TEST(NonPowerOf2ReductionTest, Size1025Int32Sum) {
  Int32Column col(1025);
  for (size_t i = 0; i < 1025; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 1025 (128 vectors + 1 tail)
  EXPECT_EQ(col.sum(), 1025) << "Size 1025 sum should be 1025";
}

// ============================================================================
// FLOAT32 TYPE WITH NON-POWER-OF-2 SIZES
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size5Float32Sum) {
  FloatColumn col(5);
  for (int i = 0; i < 5; ++i) {
    col.data()[i] = static_cast<float>(i) * 0.5f; // 0, 0.5, 1.0, 1.5, 2.0
  }
  // Expected: 0 + 0.5 + 1.0 + 1.5 + 2.0 = 5.0
  EXPECT_FLOAT_EQ(col.sum(), 5.0f) << "Size 5 float32 sum should be 5.0";
}

TEST(NonPowerOf2ReductionTest, Size5Float32Product) {
  FloatColumn col(5);
  col.data()[0] = 1.0f;
  col.data()[1] = 2.0f;
  col.data()[2] = 0.5f;
  col.data()[3] = 2.0f;
  col.data()[4] = 1.0f;
  // Expected: 1.0 * 2.0 * 0.5 * 2.0 * 1.0 = 2.0
  EXPECT_FLOAT_EQ(col.product(), 2.0f)
      << "Size 5 float32 product should be 2.0";
}

TEST(NonPowerOf2ReductionTest, Size9Float32Min) {
  FloatColumn col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = static_cast<float>(i); // 0, 1, 2, ..., 8
  }
  // Expected: 0.0
  EXPECT_FLOAT_EQ(col.min(), 0.0f) << "Size 9 float32 min should be 0.0";
}

TEST(NonPowerOf2ReductionTest, Size9Float32Max) {
  FloatColumn col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = static_cast<float>(i); // 0, 1, 2, ..., 8
  }
  // Expected: 8.0
  EXPECT_FLOAT_EQ(col.max(), 8.0f) << "Size 9 float32 max should be 8.0";
}

// ============================================================================
// BF16 TYPE WITH NON-POWER-OF-2 SIZES
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size5BF16Sum) {
  BF16Column col(5);
  for (int i = 0; i < 5; ++i) {
    col.data()[i] =
        bf16::from_float_trunc(static_cast<float>(i + 1)); // 1, 2, 3, 4, 5
  }
  // Expected: 15.0 (with BF16 precision tolerance)
  float result = col.sum().to_float();
  EXPECT_NEAR(result, 15.0f, 1.0f) << "Size 5 BF16 sum should be near 15.0";
}

TEST(NonPowerOf2ReductionTest, Size5BF16Product) {
  BF16Column col(5);
  col.data()[0] = bf16::from_float_trunc(2.0f);
  col.data()[1] = bf16::from_float_trunc(2.0f);
  col.data()[2] = bf16::from_float_trunc(2.0f);
  col.data()[3] = bf16::from_float_trunc(2.0f);
  col.data()[4] = bf16::from_float_trunc(2.0f);
  // Expected: 2^5 = 32.0
  float result = col.product().to_float();
  EXPECT_NEAR(result, 32.0f, 1.0f) << "Size 5 BF16 product should be near 32.0";
}

TEST(NonPowerOf2ReductionTest, Size9BF16Min) {
  BF16Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] =
        bf16::from_float_trunc(static_cast<float>(i)); // 0, 1, 2, ..., 8
  }
  // Expected: 0.0
  float result = col.min().to_float();
  EXPECT_NEAR(result, 0.0f, 0.1f) << "Size 9 BF16 min should be near 0.0";
}

TEST(NonPowerOf2ReductionTest, Size9BF16Max) {
  BF16Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] =
        bf16::from_float_trunc(static_cast<float>(i)); // 0, 1, 2, ..., 8
  }
  // Expected: 8.0
  float result = col.max().to_float();
  EXPECT_NEAR(result, 8.0f, 0.1f) << "Size 9 BF16 max should be near 8.0";
}

// ============================================================================
// TAIL MASK EDGE CASES WITH MULTIPLE VECTORS
//
// These test mask handling across main loop + tail boundary
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size17SumLastTailElementMasked) {
  Int32Column col(17);
  for (int i = 0; i < 17; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, ..., 17
  }
  col.present_mask().set(16, false); // Last element (in tail) masked
  // Expected: 1 + 2 + ... + 16 = 136
  EXPECT_EQ(col.sum(), 136)
      << "Size 17 sum with last tail element masked should be 136";
}

TEST(NonPowerOf2ReductionTest, Size17ProductFirstTailElementMasked) {
  Int32Column col(17);
  for (int i = 0; i < 17; ++i) {
    col.data()[i] = 2;
  }
  col.present_mask().set(8, false); // First element of tail (index 8) masked
  // Expected: 2^16 = 65536 (skip the 9th element)
  EXPECT_EQ(col.product(), 65536)
      << "Size 17 product with first tail element masked should be 2^16";
}

// ============================================================================
// BOUNDARY VALUE TESTS WITH MASKING
//
// Tests to expose min/max identity blending bugs
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size15AllTailMaskedMin) {
  Int32Column col(15);
  for (int i = 0; i < 15; ++i) {
    col.data()[i] = 100 - i; // 100, 99, ..., 86
    if (i >= 8) {
      col.present_mask().set(i, false); // Mask out tail (indices 8-14)
    }
  }
  // Expected: min of first 8 elements = 100, 99, ..., 93 = 93
  EXPECT_EQ(col.min(), 93)
      << "Size 15 min with all tail masked should use first vector";
}

TEST(NonPowerOf2ReductionTest, Size15AllTailMaskedMax) {
  Int32Column col(15);
  for (int i = 0; i < 15; ++i) {
    col.data()[i] = 50 + i; // 50, 51, ..., 64
    if (i >= 8) {
      col.present_mask().set(i, false); // Mask out tail
    }
  }
  // Expected: max of first 8 elements = 50, 51, ..., 57 = 57
  EXPECT_EQ(col.max(), 57)
      << "Size 15 max with all tail masked should use first vector";
}

// ============================================================================
// NEGATIVE NUMBER EDGE CASES WITH NON-POWER-OF-2 SIZES
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size5NegativeMin) {
  Int32Column col(5);
  col.data()[0] = -10;
  col.data()[1] = 5;
  col.data()[2] = -20;
  col.data()[3] = 0;
  col.data()[4] = 15;
  // Expected: -20
  EXPECT_EQ(col.min(), -20) << "Size 5 min should find negative minimum";
}

TEST(NonPowerOf2ReductionTest, Size5NegativeMax) {
  Int32Column col(5);
  col.data()[0] = -10;
  col.data()[1] = 5;
  col.data()[2] = -20;
  col.data()[3] = 0;
  col.data()[4] = 15;
  // Expected: 15
  EXPECT_EQ(col.max(), 15) << "Size 5 max should find positive maximum";
}

TEST(NonPowerOf2ReductionTest, Size9AllNegativeMin) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = -(i + 1); // -1, -2, -3, ..., -9
  }
  // Expected: -9 (smallest negative)
  EXPECT_EQ(col.min(), -9) << "Size 9 min should find smallest negative";
}

TEST(NonPowerOf2ReductionTest, Size9AllNegativeMax) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = -(i + 1); // -1, -2, -3, ..., -9
  }
  // Expected: -1 (largest negative, i.e., closest to 0)
  EXPECT_EQ(col.max(), -1) << "Size 9 max should find largest negative";
}

// ============================================================================
// SPARSE MASK PATTERNS (Testing mask construction correctness)
// ============================================================================

TEST(NonPowerOf2ReductionTest, Size9AlternatingMaskSum) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, 4, 5, 6, 7, 8, 9
    col.present_mask().set(i, i % 2 ==
                                  0); // Present at even indices: 0, 2, 4, 6, 8
  }
  // Expected: 1 + 3 + 5 + 7 + 9 = 25
  EXPECT_EQ(col.sum(), 25) << "Size 9 sum with alternating mask should be 25";
}

TEST(NonPowerOf2ReductionTest, Size9FirstHalfMaskedSum) {
  Int32Column col(9);
  for (int i = 0; i < 9; ++i) {
    col.data()[i] = i + 1; // 1, 2, 3, 4, 5, 6, 7, 8, 9
    if (i < 4) {
      col.present_mask().set(i, false); // Mask first 4 elements
    }
  }
  // Expected: 5 + 6 + 7 + 8 + 9 = 35
  EXPECT_EQ(col.sum(), 35) << "Size 9 sum with first half masked should be 35";
}

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
