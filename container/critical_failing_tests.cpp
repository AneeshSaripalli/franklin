// Focused tests for real edge cases with non-power-of-2 sizes
// NOTE: Padding bits (beyond num_bits_) are INDETERMINATE by design
// Use test(pos) for bounds checking, test_unchecked(pos) for raw access

#include "container/column.hpp"
#include "container/dynamic_bitset.hpp"
#include "core/error_collector.hpp"
#include <gtest/gtest.h>

namespace franklin {

struct TestPolicy {
  using size_type = std::size_t;
};
using Bitset = dynamic_bitset<TestPolicy>;

class ErrorGuard {
public:
  ErrorGuard() {
    core::ErrorCollector::instance().clear();
    core::ErrorCollector::instance().set_enabled(true);
  }
  ~ErrorGuard() { core::ErrorCollector::instance().clear(); }
};

// ============================================================================
// CRITICAL: resize() inconsistency with num_blocks() vs num_blocks_needed()
// ============================================================================

TEST(CriticalBugs, BitsetResizeBlockCountMismatch) {
  ErrorGuard guard;

  // Constructor uses num_blocks_needed() which rounds to 8-block boundaries
  // resize() uses num_blocks() which returns actual needed blocks
  // This creates an inconsistency that can corrupt internal state

  Bitset bs(64, true); // 64 bits = 1 block, but allocated as 8 blocks (rounded)

  // Internal state: blocks_.size() == 8 (from num_blocks_needed(64))
  //                 num_bits_ == 64

  EXPECT_EQ(bs.count(), 64);

  // Resize to 65 bits
  // resize() at line 103: old_num_blocks = num_blocks() = 1
  //                       new_num_blocks = num_blocks() = 2
  // It tries to resize blocks_ from 1 to 2, but blocks_.size() is actually 8!
  // This calls blocks_.resize(2), SHRINKING from 8 to 2

  bs.resize(65, true);

  EXPECT_TRUE(bs.test(64)) << "Bit 64 should be set after resize";
  EXPECT_EQ(bs.count(), 65) << "Count should be 65 after resize";

  // If blocks_ was shrunk from 8 to 2, later operations expecting 8 blocks will
  // crash Test by calling operations that touch all blocks
  EXPECT_NO_THROW(bs.set()) << "set() should not crash after resize";
  EXPECT_NO_THROW(bs.count()) << "count() should not crash after resize";
}

// ============================================================================
// Non-power-of-2 size edge cases
// ============================================================================

TEST(EdgeCases, BitsetCountPrimeSizes) {
  ErrorGuard guard;

  const std::vector<size_t> prime_sizes = {7, 13, 23, 31, 67, 127};

  for (size_t N : prime_sizes) {
    Bitset bs(N, true);
    EXPECT_EQ(bs.count(), N) << "Prime size " << N << " should count correctly";

    bs.reset();
    EXPECT_EQ(bs.count(), 0u) << "After reset, count should be 0";

    // Set every 3rd bit
    for (size_t i = 0; i < N; i += 3) {
      bs.set(i, true);
    }

    size_t expected = (N + 2) / 3; // Ceiling division
    EXPECT_EQ(bs.count(), expected)
        << "Striped pattern count mismatch for size " << N;
  }
}

TEST(EdgeCases, BitsetAllWithPartialBlocks) {
  ErrorGuard guard;

  // Test all() when remainder != 0 (last block is partial)
  const std::vector<size_t> sizes = {1, 7, 23, 63, 65, 127};

  for (size_t N : sizes) {
    Bitset bs(N, true);
    EXPECT_TRUE(bs.all()) << "Size " << N
                          << " with all bits set should return true";

    // Clear one bit and check
    if (N > 0) {
      bs.set(N / 2, false);
      EXPECT_FALSE(bs.all())
          << "Size " << N << " with one bit clear should return false";
    }
  }
}

TEST(EdgeCases, ColumnArithmeticOddSizes) {
  ErrorGuard guard;

  const std::vector<size_t> sizes = {1, 7, 9, 13, 15, 17, 23, 31, 33};

  for (size_t N : sizes) {
    column_vector<Int32DefaultPolicy> a(N);
    column_vector<Int32DefaultPolicy> b(N);

    for (size_t i = 0; i < N; ++i) {
      a.data()[i] = static_cast<int32_t>(i + 1);
      b.data()[i] = static_cast<int32_t>(i * 2);
    }

    auto sum = a + b;
    auto diff = a - b;
    auto prod = a * b;

    // Verify every element
    for (size_t i = 0; i < N; ++i) {
      EXPECT_EQ(sum.data()[i], static_cast<int32_t>((i + 1) + i * 2))
          << "Addition failed at index " << i << " for size " << N;
      EXPECT_EQ(diff.data()[i], static_cast<int32_t>((i + 1) - i * 2))
          << "Subtraction failed at index " << i << " for size " << N;
      EXPECT_EQ(prod.data()[i], static_cast<int32_t>((i + 1) * i * 2))
          << "Multiplication failed at index " << i << " for size " << N;
    }
  }
}

TEST(EdgeCases, ColumnScalarNonCommutative) {
  ErrorGuard guard;

  column_vector<Float32DefaultPolicy> col(13); // Prime size

  for (size_t i = 0; i < 13; ++i) {
    col.data()[i] = static_cast<float>(i);
  }

  // column - scalar
  auto result1 = col - 5.0f;
  for (size_t i = 0; i < 13; ++i) {
    EXPECT_FLOAT_EQ(result1.data()[i], static_cast<float>(i) - 5.0f);
  }

  // scalar - column (non-commutative!)
  auto result2 = 10.0f - col;
  for (size_t i = 0; i < 13; ++i) {
    EXPECT_FLOAT_EQ(result2.data()[i], 10.0f - static_cast<float>(i))
        << "scalar - column failed at index " << i;
  }
}

TEST(EdgeCases, ColumnBitmaskPrimeSize) {
  ErrorGuard guard;

  column_vector<Int32DefaultPolicy> a(17); // Prime
  column_vector<Int32DefaultPolicy> b(17);

  for (size_t i = 0; i < 17; ++i) {
    a.data()[i] = static_cast<int32_t>(i);
    b.data()[i] = static_cast<int32_t>(i * 2);
  }

  // Mark some as missing with non-trivial pattern
  a.present_mask().set(0, false);
  a.present_mask().set(8, false);
  a.present_mask().set(16, false); // Last element

  b.present_mask().set(1, false);
  b.present_mask().set(8, false); // Overlap with a
  b.present_mask().set(15, false);

  auto result = a + b;

  // Verify bitmask intersection for every element
  for (size_t i = 0; i < 17; ++i) {
    bool expected = a.present(i) && b.present(i);
    EXPECT_EQ(result.present(i), expected) << "Bitmask mismatch at index " << i;
  }

  // Specifically check overlaps and edges
  EXPECT_FALSE(result.present(0));  // Missing in a
  EXPECT_FALSE(result.present(1));  // Missing in b
  EXPECT_FALSE(result.present(8));  // Missing in both
  EXPECT_FALSE(result.present(15)); // Missing in b
  EXPECT_FALSE(result.present(16)); // Missing in a, last element
}

TEST(EdgeCases, BF16CacheLineBoundaries) {
  ErrorGuard guard;

  // BF16 has 32 elements per cache line (64 bytes / 2 bytes)
  const std::vector<size_t> sizes = {31, 32, 33, 63, 64, 65};

  for (size_t N : sizes) {
    column_vector<BF16DefaultPolicy> a(N);
    column_vector<BF16DefaultPolicy> b(N);

    for (size_t i = 0; i < N; ++i) {
      a.data()[i] = bf16::from_float(static_cast<float>(i + 1));
      b.data()[i] = bf16::from_float(3.0f);
    }

    auto result = a * b;

    // Verify every element (BF16 has reduced precision)
    for (size_t i = 0; i < N; ++i) {
      float expected = static_cast<float>(i + 1) * 3.0f;
      float actual = result.data()[i].to_float();
      EXPECT_NEAR(actual, expected, 0.5f)
          << "BF16 multiply failed at index " << i << " for size " << N;
    }
  }
}

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
