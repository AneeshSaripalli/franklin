#include "container/dynamic_bitset.hpp"
#include "core/error_collector.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>

using namespace franklin::core;

namespace franklin {

struct DefaultPolicy {
  using size_type = std::size_t;
};

using Bitset = dynamic_bitset<DefaultPolicy>;

class ErrorGuard {
public:
  ErrorGuard() {
    ErrorCollector::instance().clear();
    ErrorCollector::instance().set_enabled(true);
  }
  ~ErrorGuard() { ErrorCollector::instance().clear(); }
};

// ============================================================================
// FAILING TEST #16: Repeated resize alternating grow/shrink corrupts state
// ============================================================================

TEST(DynamicBitsetStress, AlternatingResizeCorruption) {
  ErrorGuard guard;

  Bitset bs(100, false);

  // Set a specific pattern
  for (size_t i = 0; i < 100; i += 10) {
    bs.set(i);
  }
  EXPECT_EQ(bs.count(), 10);

  // Repeatedly grow and shrink
  for (int iter = 0; iter < 100; ++iter) {
    bs.resize(150, false); // Grow
    bs.resize(100, false); // Shrink back
  }

  // EXPECTED: Original pattern should be preserved
  // ACTUAL: May be corrupted if resize doesn't handle shrinking correctly

  EXPECT_EQ(bs.size(), 100);

  for (size_t i = 0; i < 100; i += 10) {
    EXPECT_TRUE(bs.test(i))
        << "Bit " << i << " should still be set after resize cycles";
  }

  EXPECT_EQ(bs.count(), 10) << "Count corrupted after resize cycles";
}

// ============================================================================
// FAILING TEST #17: Resize to zero and back loses state
// ============================================================================

TEST(DynamicBitsetStress, ResizeToZeroAndBack) {
  ErrorGuard guard;

  Bitset bs(50, false);
  bs.set(10);
  bs.set(20);
  bs.set(30);
  EXPECT_EQ(bs.count(), 3);

  // Resize to 0
  bs.resize(0);
  EXPECT_EQ(bs.size(), 0);
  EXPECT_TRUE(bs.empty());
  EXPECT_EQ(bs.count(), 0);

  // Resize back to 50
  bs.resize(50, false);

  // EXPECTED: All bits should be clear (we lost the old state)
  // ACTUAL: Depends on whether blocks_ was cleared or reused

  EXPECT_EQ(bs.size(), 50);
  EXPECT_EQ(bs.count(), 0)
      << "After resize to 0 and back, all bits should be clear";

  for (size_t i = 0; i < 50; ++i) {
    EXPECT_FALSE(bs.test(i)) << "Bit " << i << " should be clear";
  }
}

// ============================================================================
// FAILING TEST #18: push_back across multiple block boundaries
// ============================================================================

TEST(DynamicBitsetStress, PushBackAcrossManyBlocks) {
  ErrorGuard guard;

  Bitset bs;

  // Push 200 bits (crosses 3 blocks: 0-63, 64-127, 128-191, 192-199)
  for (size_t i = 0; i < 200; ++i) {
    bs.push_back(i % 2 == 0); // Alternating pattern
  }

  EXPECT_EQ(bs.size(), 200);
  EXPECT_EQ(bs.count(), 100);

  // Verify pattern
  for (size_t i = 0; i < 200; ++i) {
    bool expected = (i % 2 == 0);
    EXPECT_EQ(bs.test(i), expected)
        << "Bit " << i << " has wrong value after push_back";
  }

  // Verify specifically at block boundaries
  EXPECT_TRUE(bs.test(62));  // Even, should be true
  EXPECT_FALSE(bs.test(63)); // Odd, should be false
  EXPECT_TRUE(bs.test(64));  // Even, should be true
  EXPECT_FALSE(bs.test(65)); // Odd, should be false

  EXPECT_TRUE(bs.test(126));  // Even
  EXPECT_FALSE(bs.test(127)); // Odd
  EXPECT_TRUE(bs.test(128));  // Even
}

// ============================================================================
// FAILING TEST #19: Flip all bits preserves count correctly
// ============================================================================

TEST(DynamicBitsetStress, FlipAllPreservesCount) {
  ErrorGuard guard;

  // Test with various sizes around block boundaries
  for (size_t size : {1, 63, 64, 65, 127, 128, 129, 200}) {
    Bitset bs(size, false);

    // Set half the bits
    for (size_t i = 0; i < size; i += 2) {
      bs.set(i);
    }

    size_t expected_count = (size + 1) / 2; // Ceiling division
    EXPECT_EQ(bs.count(), expected_count)
        << "Initial count wrong for size " << size;

    // Flip all bits
    bs.flip();

    // EXPECTED: Count should be (size - expected_count)
    // ACTUAL: If flip() doesn't call zero_unused_bits(), may count garbage bits

    size_t flipped_count = size - expected_count;
    EXPECT_EQ(bs.count(), flipped_count)
        << "Count after flip wrong for size " << size << " (expected "
        << flipped_count << ", got " << bs.count() << ")";
  }
}

// ============================================================================
// FAILING TEST #20: set() all bits then flip specific bits at boundaries
// ============================================================================

TEST(DynamicBitsetStress, SetAllThenFlipBoundaries) {
  ErrorGuard guard;

  Bitset bs(200, false);

  // Set all bits
  bs.set();
  EXPECT_EQ(bs.count(), 200);
  EXPECT_TRUE(bs.all());

  // Flip bits exactly at block boundaries
  bs.flip(0);
  bs.flip(63);
  bs.flip(64);
  bs.flip(127);
  bs.flip(128);
  bs.flip(191);

  EXPECT_EQ(bs.count(), 194) << "Should have 6 bits cleared (200 - 6 = 194)";
  EXPECT_FALSE(bs.all());

  // Verify the flipped bits
  EXPECT_FALSE(bs.test(0));
  EXPECT_FALSE(bs.test(63));
  EXPECT_FALSE(bs.test(64));
  EXPECT_FALSE(bs.test(127));
  EXPECT_FALSE(bs.test(128));
  EXPECT_FALSE(bs.test(191));

  // Verify adjacent bits are still set
  EXPECT_TRUE(bs.test(1));
  EXPECT_TRUE(bs.test(62));
  EXPECT_TRUE(bs.test(65));
  EXPECT_TRUE(bs.test(126));
  EXPECT_TRUE(bs.test(129));
  EXPECT_TRUE(bs.test(190));
}

// ============================================================================
// FAILING TEST #21: Bitwise NOT preserves size and inverts correctly
// ============================================================================

TEST(DynamicBitsetStress, BitwiseNotPreservesSize) {
  ErrorGuard guard;

  Bitset bs(100, false);
  bs.set(10);
  bs.set(50);
  bs.set(90);

  EXPECT_EQ(bs.count(), 3);

  // Create inverted copy
  Bitset inverted = ~bs;

  EXPECT_EQ(inverted.size(), 100) << "Inverted bitset should have same size";
  EXPECT_EQ(inverted.count(), 97)
      << "Inverted should have 97 bits set (100 - 3)";

  // Verify inversion
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_NE(bs.test(i), inverted.test(i))
        << "Bit " << i << " should be inverted";
  }

  // Double inversion should give original
  Bitset double_inv = ~inverted;
  EXPECT_EQ(double_inv.size(), 100);
  EXPECT_EQ(double_inv.count(), 3);

  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(bs.test(i), double_inv.test(i))
        << "Double inversion should restore original";
  }
}

// ============================================================================
// FAILING TEST #22: Multiple bitwise operations chained
// ============================================================================

TEST(DynamicBitsetStress, ChainedBitwiseOperations) {
  ErrorGuard guard;

  Bitset a(100, false);
  Bitset b(100, false);
  Bitset c(100, false);

  // Set different patterns
  for (size_t i = 0; i < 100; i += 2)
    a.set(i); // Even bits
  for (size_t i = 1; i < 100; i += 2)
    b.set(i); // Odd bits
  for (size_t i = 0; i < 100; i += 3)
    c.set(i); // Every 3rd bit

  EXPECT_EQ(a.count(), 50);
  EXPECT_EQ(b.count(), 50);
  EXPECT_EQ(c.count(), 34);

  // Test De Morgan's law: ~(a & b) == (~a | ~b)
  Bitset lhs = ~(a & b);
  Bitset temp_a = ~a;
  Bitset temp_b = ~b;
  Bitset rhs = temp_a | temp_b;

  EXPECT_EQ(lhs.size(), rhs.size());
  EXPECT_EQ(lhs.count(), rhs.count()) << "De Morgan's law violated in count";

  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(lhs.test(i), rhs.test(i))
        << "De Morgan's law violated at bit " << i;
  }

  // Test distributivity: a & (b | c) == (a & b) | (a & c)
  Bitset lhs2 = a & (b | c);
  Bitset rhs2 = (a & b) | (a & c);

  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(lhs2.test(i), rhs2.test(i))
        << "Distributivity violated at bit " << i;
  }
}

// ============================================================================
// FAILING TEST #23: XOR with different-sized bitsets
// ============================================================================

TEST(DynamicBitsetStress, XorDifferentSizes) {
  ErrorGuard guard;

  Bitset bs1(100, false);
  Bitset bs2(50, false);

  bs1.set(10);
  bs1.set(70);
  bs2.set(10);
  bs2.set(30);

  bs1 ^= bs2;

  // EXPECTED:
  // - Bit 10: 1 XOR 1 = 0 (both had it)
  // - Bit 30: 0 XOR 1 = 1 (only bs2 had it, but bs2 is shorter so it processes)
  // - Bit 70: 1 XOR 0 = 1 (only bs1 had it, beyond bs2 range)
  //
  // ACTUAL: operator^= uses min_blocks, so bit 70 is unchanged (correct for
  // XOR)
  //         but semantically surprising

  EXPECT_EQ(bs1.size(), 100);
  EXPECT_FALSE(bs1.test(10)) << "10 XOR 10 should clear bit 10";
  EXPECT_TRUE(bs1.test(30)) << "0 XOR 30 should set bit 30";
  EXPECT_TRUE(bs1.test(70)) << "70 XOR (nothing) should keep bit 70";
}

// ============================================================================
// FAILING TEST #24: Random operations stress test
// ============================================================================

TEST(DynamicBitsetStress, RandomOperationsStressTest) {
  ErrorGuard guard;

  std::mt19937 rng(12345); // Fixed seed for reproducibility
  std::uniform_int_distribution<size_t> pos_dist(0, 999);
  std::uniform_int_distribution<int> op_dist(0, 4);

  Bitset bs(1000, false);
  std::vector<bool> reference(1000, false); // Reference implementation

  // Perform 10000 random operations
  for (int i = 0; i < 10000; ++i) {
    size_t pos = pos_dist(rng);
    int op = op_dist(rng);

    switch (op) {
    case 0: // set
      bs.set(pos);
      reference[pos] = true;
      break;
    case 1: // reset
      bs.reset(pos);
      reference[pos] = false;
      break;
    case 2: // flip
      bs.flip(pos);
      reference[pos] = !reference[pos];
      break;
    case 3: // test
      EXPECT_EQ(bs.test(pos), reference[pos])
          << "Mismatch at position " << pos << " after " << i << " operations";
      break;
    case 4: // count check
      size_t expected_count =
          std::count(reference.begin(), reference.end(), true);
      EXPECT_EQ(bs.count(), expected_count)
          << "Count mismatch after " << i << " operations";
      break;
    }
  }

  // Final verification
  for (size_t i = 0; i < 1000; ++i) {
    EXPECT_EQ(bs.test(i), reference[i]) << "Final state mismatch at bit " << i;
  }
}

// ============================================================================
// FAILING TEST #25: Resize larger then smaller repeatedly with value=true
// ============================================================================

TEST(DynamicBitsetStress, ResizeLargerSmallerWithTrue) {
  ErrorGuard guard;

  Bitset bs(64, false);
  bs.set(0);
  bs.set(63);

  // Grow with value=true
  bs.resize(128, true);

  // EXPECTED: bits 0 and 63 still set, bits 64-127 all set
  // ACTUAL: May fail if resize doesn't set new bits correctly

  EXPECT_EQ(bs.size(), 128);
  EXPECT_TRUE(bs.test(0));
  EXPECT_TRUE(bs.test(63));

  for (size_t i = 64; i < 128; ++i) {
    EXPECT_TRUE(bs.test(i))
        << "Bit " << i << " should be set after resize with value=true";
  }

  EXPECT_EQ(bs.count(), 66) << "Should have 66 bits set (2 old + 64 new)";

  // Shrink back
  bs.resize(64);

  // EXPECTED: Only bits 0 and 63 remain set
  // ACTUAL: May have stale bits if shrink doesn't clear

  EXPECT_EQ(bs.count(), 2) << "After shrinking, should only have 2 bits set";
}

// ============================================================================
// FAILING TEST #26: Clear after operations restores empty state
// ============================================================================

TEST(DynamicBitsetStress, ClearRestoresEmptyState) {
  ErrorGuard guard;

  Bitset bs(200, true);
  EXPECT_EQ(bs.count(), 200);
  EXPECT_FALSE(bs.empty());

  // Perform various operations
  bs.flip();
  bs.set(50);
  bs.resize(300, true);

  EXPECT_EQ(bs.size(), 300);
  EXPECT_FALSE(bs.empty());

  // Clear
  bs.clear();

  EXPECT_EQ(bs.size(), 0);
  EXPECT_TRUE(bs.empty());
  EXPECT_EQ(bs.count(), 0);
  EXPECT_TRUE(bs.none());
  EXPECT_TRUE(bs.all()); // Vacuous truth for empty

  // Should be safe to operate on cleared bitset
  bs.set();   // No-op
  bs.reset(); // No-op
  bs.flip();  // No-op

  EXPECT_EQ(bs.count(), 0);
}

// ============================================================================
// FAILING TEST #27: Bit patterns at exact multiples of 64
// ============================================================================

TEST(DynamicBitsetStress, ExactMultiplesOf64) {
  ErrorGuard guard;

  // Test sizes that are exact multiples of 64
  for (size_t size : {64, 128, 192, 256, 320}) {
    Bitset bs(size, false);

    // Set all bits
    bs.set();
    EXPECT_EQ(bs.count(), size) << "Count wrong for size " << size;
    EXPECT_TRUE(bs.all()) << "all() should be true for size " << size;

    // Flip all
    bs.flip();
    EXPECT_EQ(bs.count(), 0)
        << "Count should be 0 after flip for size " << size;
    EXPECT_TRUE(bs.none()) << "none() should be true after flip for size "
                           << size;

    // Test boundary bit
    bs.set(size - 1);
    EXPECT_EQ(bs.count(), 1);
    EXPECT_TRUE(bs.test(size - 1));
  }
}

// ============================================================================
// FAILING TEST #28: any() with single bit in last block
// ============================================================================

TEST(DynamicBitsetStress, AnyWithSingleBitInLastBlock) {
  ErrorGuard guard;

  Bitset bs(129, false); // 3 blocks, last block has 1 bit used

  EXPECT_TRUE(bs.none());
  EXPECT_FALSE(bs.any());

  // Set only the last bit
  bs.set(128);

  EXPECT_FALSE(bs.none());
  EXPECT_TRUE(bs.any());
  EXPECT_EQ(bs.count(), 1);

  // Clear it
  bs.reset(128);

  EXPECT_TRUE(bs.none());
  EXPECT_FALSE(bs.any());
  EXPECT_EQ(bs.count(), 0);
}

// ============================================================================
// FAILING TEST #29: OR with self is idempotent
// ============================================================================

TEST(DynamicBitsetStress, OrWithSelfIdempotent) {
  ErrorGuard guard;

  Bitset bs(100, false);
  bs.set(10);
  bs.set(50);
  bs.set(90);

  size_t original_count = bs.count();

  // OR with self should not change anything
  bs |= bs;

  EXPECT_EQ(bs.count(), original_count);
  EXPECT_TRUE(bs.test(10));
  EXPECT_TRUE(bs.test(50));
  EXPECT_TRUE(bs.test(90));
}

// ============================================================================
// FAILING TEST #30: AND with self is idempotent
// ============================================================================

TEST(DynamicBitsetStress, AndWithSelfIdempotent) {
  ErrorGuard guard;

  Bitset bs(100, false);
  bs.set(10);
  bs.set(50);
  bs.set(90);

  size_t original_count = bs.count();

  // AND with self should not change anything
  bs &= bs;

  EXPECT_EQ(bs.count(), original_count);
  EXPECT_TRUE(bs.test(10));
  EXPECT_TRUE(bs.test(50));
  EXPECT_TRUE(bs.test(90));
}

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
