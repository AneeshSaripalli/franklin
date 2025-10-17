#include "container/dynamic_bitset.hpp"
#include "core/error_collector.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

// Bring error collector into franklin namespace for compatibility
using namespace franklin::core;

namespace franklin {

// Test policy with std::size_t
struct DefaultPolicy {
  using size_type = std::size_t;
};

// Test policy with uint32_t to test smaller size types
struct SmallSizePolicy {
  using size_type = std::uint32_t;
};

using Bitset = dynamic_bitset<DefaultPolicy>;
using SmallBitset = dynamic_bitset<SmallSizePolicy>;

// RAII helper to clear errors before/after each test
class ErrorGuard {
public:
  ErrorGuard() {
    core::ErrorCollector::instance().clear();
    core::ErrorCollector::instance().set_enabled(true);
  }

  ~ErrorGuard() { core::ErrorCollector::instance().clear(); }

  bool has_errors() const {
    return core::ErrorCollector::instance().has_errors();
  }

  size_t error_count() const {
    return core::ErrorCollector::instance().error_count();
  }

  core::ErrorInfo last_error() const {
    return core::ErrorCollector::instance().get_last_error();
  }
};

// ============================================================================
// FAILING TEST #1: Resize within same block corrupts existing bits
// ============================================================================

TEST(DynamicBitsetTest, ResizeWithinBlockCorruptsExistingBits) {
  ErrorGuard guard;

  // Create bitset with 10 bits, set specific pattern
  Bitset bs(10, false);
  bs.set(0); // bit 0 = 1
  bs.set(2); // bit 2 = 1
  bs.set(5); // bit 5 = 1
  // Pattern: 0010010001

  EXPECT_TRUE(bs.test(0));
  EXPECT_TRUE(bs.test(2));
  EXPECT_TRUE(bs.test(5));
  EXPECT_EQ(bs.count(), 3);

  // Resize to 15 bits (still same block), value=false
  bs.resize(15, false);

  // EXPECTED: bits 0, 2, 5 should still be set, bits 10-14 should be clear
  // ACTUAL: The resize implementation doesn't preserve bits when new_num_blocks
  // == old_num_blocks
  //         It only calls zero_unused_bits() which operates on the LAST block
  //         based on num_bits_ but doesn't initialize the new bits in range
  //         [10, 15)

  EXPECT_EQ(bs.size(), 15) << "Size should be 15";
  EXPECT_TRUE(bs.test(0)) << "Bit 0 should still be set";
  EXPECT_TRUE(bs.test(2)) << "Bit 2 should still be set";
  EXPECT_TRUE(bs.test(5)) << "Bit 5 should still be set";

  // These might fail - new bits have undefined state
  for (size_t i = 10; i < 15; ++i) {
    EXPECT_FALSE(bs.test(i))
        << "New bit " << i << " should be false but has undefined state";
  }

  EXPECT_EQ(bs.count(), 3) << "Should still have exactly 3 bits set, but may "
                              "have garbage in new bits";
}

// ============================================================================
// FAILING TEST #2: Resize with value=true within same block doesn't set new
// bits
// ============================================================================

TEST(DynamicBitsetTest, ResizeWithinBlockValueTrueDoesntSetNewBits) {
  ErrorGuard guard;

  Bitset bs(10, false);
  bs.set(0);
  bs.set(5);
  EXPECT_EQ(bs.count(), 2);

  // Resize to 20 bits with value=true - should set bits 10-19
  bs.resize(20, true);

  EXPECT_EQ(bs.size(), 20);

  // Old bits should be preserved
  EXPECT_TRUE(bs.test(0));
  EXPECT_TRUE(bs.test(5));

  // EXPECTED: New bits [10, 20) should be set to true
  // ACTUAL: resize() only calls zero_unused_bits() when value=true
  //         This CLEARS unused bits instead of setting new bits to true
  //         The blocks_.resize() is never called when num_blocks stays the same

  for (size_t i = 10; i < 20; ++i) {
    EXPECT_TRUE(bs.test(i))
        << "New bit " << i << " should be true but resize doesn't set it";
  }
}

// ============================================================================
// FAILING TEST #3: Shrinking bitset doesn't clear bits beyond new size
// ============================================================================

TEST(DynamicBitsetTest, ShrinkDoesntClearBitsInUnusedBlock) {
  ErrorGuard guard;

  // Create 70-bit bitset (2 blocks), set bit 65
  Bitset bs(70, false);
  bs.set(65);
  EXPECT_TRUE(bs.test(65));
  EXPECT_EQ(bs.count(), 1);

  // Shrink to 60 bits (still 1 block)
  bs.resize(60);

  EXPECT_EQ(bs.size(), 60);

  // EXPECTED: bit 65 should be cleared since it's beyond new size
  // ACTUAL: resize() doesn't clear bits beyond new size when shrinking within
  // same block
  //         The block still contains the old bit value
  //         This violates the invariant that unused bits should be zero

  // Try to access via operator[] (unchecked) - this reveals internal state
  // Block 1 should have bit 1 (which is position 65) cleared
  EXPECT_EQ(bs.count(), 0)
      << "Count should be 0 but stale bit 65 is still set in the block";
}

// ============================================================================
// FAILING TEST #4: Bitwise AND with different sizes silently truncates
// ============================================================================

TEST(DynamicBitsetTest, BitwiseAndTruncatesSilently) {
  ErrorGuard guard;

  // Create two bitsets of different sizes
  Bitset bs1(100, false);
  Bitset bs2(50, false);

  // Set bits in both
  bs1.set(10);
  bs1.set(60); // Beyond bs2's size
  bs2.set(10);

  EXPECT_EQ(bs1.count(), 2);
  EXPECT_EQ(bs2.count(), 1);

  // Perform AND operation
  bs1 &= bs2;

  // EXPECTED: Should either:
  //   (1) Report error about size mismatch, OR
  //   (2) Treat bs2 as having infinite zeros beyond its size
  // ACTUAL: Uses min(blocks.size()) and silently truncates
  //         Bits beyond min size are left unchanged in bs1

  EXPECT_EQ(bs1.size(), 100) << "bs1 size unchanged";
  EXPECT_TRUE(bs1.test(10)) << "Bit 10 should be true (both had it set)";

  // This exposes the bug: bit 60 should be cleared (bs2 doesn't have it)
  // but operator&= only processes min_blocks, leaving bit 60 unchanged
  EXPECT_FALSE(bs1.test(60)) << "Bit 60 should be false (bs2 implicitly has 0 "
                                "there) but operator&= doesn't touch it";
}

// ============================================================================
// FAILING TEST #5: Bitwise OR with different sizes doesn't set bits from larger
// operand
// ============================================================================

TEST(DynamicBitsetTest, BitwiseOrIgnoresLargerOperandBits) {
  ErrorGuard guard;

  Bitset bs1(50, false);
  Bitset bs2(100, false);

  bs1.set(10);
  bs2.set(10);
  bs2.set(70); // Beyond bs1's size

  // bs1 |= bs2 should logically set bit 70 in bs1 (growing it?)
  // or report an error about size mismatch
  bs1 |= bs2;

  // EXPECTED: Either grow bs1 to include bit 70, or report error
  // ACTUAL: operator|= uses min_blocks, so bit 70 from bs2 is completely
  // ignored

  EXPECT_EQ(bs1.size(), 50);
  EXPECT_TRUE(bs1.test(10));

  // This is the semantic bug: bs1 | bs2 should have bit 70 set,
  // but since bs1 is smaller, it's silently ignored
  // This test documents the current behavior as incorrect
  EXPECT_EQ(bs1.count(), 1)
      << "Only bit 10 is set; bit 70 from bs2 was silently ignored";
}

// ============================================================================
// FAILING TEST #6: all() returns true for empty bitset (debatable but
// suspicious)
// ============================================================================

TEST(DynamicBitsetTest, AllOnEmptyBitsetReturnsTrue) {
  ErrorGuard guard;

  Bitset bs(0);

  // EXPECTED: Unclear - vacuous truth suggests true, but it's counterintuitive
  // ACTUAL: Returns true (line 175: if (empty()) return true)
  //
  // This is mathematically correct but semantically confusing:
  // "Are all bits set?" when there are no bits to check
  bool result = bs.all();

  EXPECT_TRUE(result) << "all() returns true for empty bitset - this is "
                         "vacuous truth but may surprise users";

  // More problematic: any() returns false for empty
  EXPECT_FALSE(bs.any());
  EXPECT_TRUE(bs.none());

  // So we have: all() && none() both true for empty bitset
  // This violates intuition that all() and none() should be opposites
  EXPECT_TRUE(bs.all() && bs.none())
      << "Empty bitset has both all() and none() true - confusing API";
}

// ============================================================================
// FAILING TEST #7: all() iterates blocks_.size() - 1 which crashes on empty
// ============================================================================

TEST(DynamicBitsetTest, AllOnEmptyVectorHasOffByOne) {
  ErrorGuard guard;

  // This tests an implementation detail that's dangerous
  // Create empty bitset
  Bitset bs(0);

  // all() has early return for empty(), so this is safe
  EXPECT_TRUE(bs.all());

  // But what if we create a bitset with 64 bits exactly, then shrink to 0?
  Bitset bs2(64, true);
  bs2.resize(0);

  EXPECT_EQ(bs2.size(), 0);
  EXPECT_TRUE(bs2.empty());

  // EXPECTED: Should handle empty case
  // ACTUAL: all() line 177 does "for (size_type i = 0; i < blocks_.size() - 1;
  // ++i)"
  //         If blocks_.size() == 0, this is undefined behavior (0 - 1 wraps to
  //         SIZE_MAX) However, the early return on line 175 prevents this
  //
  // BUT: If blocks_ is size 1, the loop doesn't run, and we only check
  // blocks_.back() This is correct but fragile - removing the empty() check
  // would cause UB

  EXPECT_TRUE(bs2.all()) << "Should handle empty after resize";
  EXPECT_TRUE(bs2.none());
}

// ============================================================================
// FAILING TEST #8: Block boundary off-by-one errors
// ============================================================================

TEST(DynamicBitsetTest, BlockBoundaryOffByOne) {
  ErrorGuard guard;

  // Test bits exactly at block boundaries: 0, 63, 64, 127, 128
  Bitset bs(200, false);

  bs.set(0);
  bs.set(63);  // Last bit of block 0
  bs.set(64);  // First bit of block 1
  bs.set(127); // Last bit of block 1
  bs.set(128); // First bit of block 2

  EXPECT_TRUE(bs.test(0));
  EXPECT_TRUE(bs.test(63));
  EXPECT_TRUE(bs.test(64));
  EXPECT_TRUE(bs.test(127));
  EXPECT_TRUE(bs.test(128));

  // Test adjacent bits are NOT set
  EXPECT_FALSE(bs.test(1));
  EXPECT_FALSE(bs.test(62));
  EXPECT_FALSE(bs.test(65));
  EXPECT_FALSE(bs.test(126));
  EXPECT_FALSE(bs.test(129));

  EXPECT_EQ(bs.count(), 5);

  // Now flip and test
  bs.flip(63);
  bs.flip(64);

  EXPECT_FALSE(bs.test(63));
  EXPECT_FALSE(bs.test(64));
  EXPECT_EQ(bs.count(), 3);

  // Reset block boundaries
  bs.reset(0);
  bs.reset(127);
  bs.reset(128);

  EXPECT_EQ(bs.count(), 0);
  EXPECT_TRUE(bs.none());

  // EXPECTED: All operations work correctly at boundaries
  // ACTUAL: This test should pass, but documents critical boundary conditions
  //         If bit_mask() or block_index() have off-by-one errors, this catches
  //         them
}

// ============================================================================
// FAILING TEST #9: zero_unused_bits mask overflow on 64-bit boundary
// ============================================================================

TEST(DynamicBitsetTest, ZeroUnusedBitsOverflowAt64Bits) {
  ErrorGuard guard;

  // When num_bits_ % 64 == 0, zero_unused_bits should be a no-op
  Bitset bs(64, false);

  // Set all bits
  bs.set();
  EXPECT_EQ(bs.count(), 64);
  EXPECT_TRUE(bs.all());

  // Flip all bits
  bs.flip();
  EXPECT_EQ(bs.count(), 0);
  EXPECT_TRUE(bs.none());

  // Now test with 128 bits (exactly 2 blocks)
  Bitset bs2(128, true);
  EXPECT_EQ(bs2.count(), 128);
  EXPECT_TRUE(bs2.all());

  // Flip and check
  bs2.flip();
  EXPECT_EQ(bs2.count(), 0);
  EXPECT_TRUE(bs2.none());

  // EXPECTED: Operations work correctly when num_bits is multiple of 64
  // ACTUAL: zero_unused_bits() line 38 checks "if (num_bits_ % bits_per_block
  // != 0)"
  //         When num_bits_ == 64, this is 0, so zero_unused_bits is skipped -
  //         CORRECT But if mask calculation is wrong, it could overflow: Line
  //         40: block_type mask = (block_type(1) << used_bits) - 1 If used_bits
  //         == 64, this would be (1 << 64) which is UB in C++ However, the
  //         if-check prevents this case
}

// ============================================================================
// FAILING TEST #10: Partial block handling in count()
// ============================================================================

TEST(DynamicBitsetTest, CountWithPartialBlockUnusedBits) {
  ErrorGuard guard;

  // Create 65-bit bitset (2 blocks, second has 1 bit used)
  Bitset bs(65, false);

  // Set all bits including those that should be unused
  bs.set();

  EXPECT_EQ(bs.count(), 65) << "Count should be exactly 65";
  EXPECT_EQ(bs.size(), 65);

  // EXPECTED: zero_unused_bits() should have cleared bits 1-63 in block 1
  // ACTUAL: set() calls zero_unused_bits(), so this should work
  //         But if zero_unused_bits() is buggy, count() will be wrong

  // Verify unused bits are actually zero by checking internal state
  // We can't access blocks_ directly, but we can infer from count()

  // Now create a 70-bit bitset
  Bitset bs2(70, true);
  EXPECT_EQ(bs2.count(), 70);

  // Resize to 65 (shrinking within block 1)
  bs2.resize(65);

  // EXPECTED: bits 65-69 should be cleared
  // ACTUAL: resize() doesn't clear bits when shrinking within same block count
  EXPECT_EQ(bs2.count(), 65)
      << "After shrinking, count should be 65 but stale bits remain";
}

// ============================================================================
// FAILING TEST #11: push_back doesn't preserve bits correctly across block
// boundary
// ============================================================================

TEST(DynamicBitsetTest, PushBackAcrossBlockBoundary) {
  ErrorGuard guard;

  // Fill first block completely
  Bitset bs(63, false);
  for (size_t i = 0; i < 63; ++i) {
    bs.set(i);
  }
  EXPECT_EQ(bs.count(), 63);

  // Push one more bit to reach 64 (still in block 0)
  bs.push_back(true);
  EXPECT_EQ(bs.size(), 64);
  EXPECT_EQ(bs.count(), 64);
  EXPECT_TRUE(bs.test(63));

  // Push another bit (this crosses into block 1)
  bs.push_back(true);
  EXPECT_EQ(bs.size(), 65);

  // EXPECTED: All 65 bits should be set
  // ACTUAL: push_back calls resize() which may not handle the block boundary
  // correctly
  EXPECT_EQ(bs.count(), 65) << "push_back across block boundary may lose bits";
  EXPECT_TRUE(bs.test(64)) << "Bit 64 (first bit of block 1) should be set";

  // Verify bits 0-63 are still set
  for (size_t i = 0; i < 64; ++i) {
    EXPECT_TRUE(bs.test(i))
        << "Bit " << i << " should still be set after push_back";
  }
}

// ============================================================================
// FAILING TEST #12: Out-of-range access doesn't report errors consistently
// ============================================================================

TEST(DynamicBitsetTest, OutOfRangeErrorReporting) {
  ErrorGuard guard;

  Bitset bs(10, false);

  // test() should report error
  EXPECT_FALSE(bs.test(10));
  EXPECT_TRUE(guard.has_errors());
  EXPECT_EQ(guard.error_count(), 1);

  auto err = guard.last_error();
  EXPECT_EQ(err.code, core::ErrorCode::OutOfRange);
  EXPECT_EQ(err.component, "dynamic_bitset");
  EXPECT_EQ(err.operation, "test");

  // Clear errors
  core::ErrorCollector::instance().clear();

  // set() should report error
  bs.set(10);
  EXPECT_TRUE(guard.has_errors());
  EXPECT_EQ(guard.error_count(), 1);

  core::ErrorCollector::instance().clear();

  // flip() should report error
  bs.flip(10);
  EXPECT_TRUE(guard.has_errors());
  EXPECT_EQ(guard.error_count(), 1);

  core::ErrorCollector::instance().clear();

  // operator[] does NOT check bounds (by design, but risky)
  // This is undefined behavior but won't report error
  bool unchecked = bs[10]; // UB: accesses blocks_[1] which doesn't exist
  EXPECT_FALSE(guard.has_errors())
      << "operator[] is unchecked and doesn't report errors";

  // EXPECTED: All checked operations report errors consistently
  // ACTUAL: This test should pass, documenting the API contract
  //         However, operator[] is a landmine for users
}

// ============================================================================
// FAILING TEST #13: Multiple errors accumulate correctly
// ============================================================================

TEST(DynamicBitsetTest, MultipleErrorAccumulation) {
  ErrorGuard guard;

  Bitset bs(5, false);

  // Generate multiple errors
  bs.test(10);
  bs.test(20);
  bs.set(30);
  bs.flip(40);

  EXPECT_EQ(guard.error_count(), 4);

  auto errors = core::ErrorCollector::instance().get_all_errors();
  EXPECT_EQ(errors.size(), 4);

  // All should be OutOfRange errors
  for (const auto& err : errors) {
    EXPECT_EQ(err.code, core::ErrorCode::OutOfRange);
    EXPECT_EQ(err.component, "dynamic_bitset");
  }

  // Check operations are distinct
  EXPECT_EQ(errors[0].operation, "test");
  EXPECT_EQ(errors[1].operation, "test");
  EXPECT_EQ(errors[2].operation, "set");
  EXPECT_EQ(errors[3].operation, "flip");

  // EXPECTED: Error context should include position and size
  // ACTUAL: Check that context_data is populated
  EXPECT_GE(errors[0].num_context_values, 2)
      << "Should have at least pos and size in context";
}

// ============================================================================
// FAILING TEST #14: Bitwise operations preserve size of left operand
// incorrectly
// ============================================================================

TEST(DynamicBitsetTest, BitwiseOpsPreserveLHSSize) {
  ErrorGuard guard;

  Bitset bs1(100, false);
  Bitset bs2(50, false);

  bs1.set(10);
  bs1.set(70);
  bs2.set(10);
  bs2.set(30);

  size_t orig_size = bs1.size();

  bs1 &= bs2;
  EXPECT_EQ(bs1.size(), orig_size) << "Size should be preserved";

  // EXPECTED: bs1 &= bs2 with different sizes should either:
  //   (1) Treat bs2 as having infinite zeros and clear all bits in bs1 beyond
  //   bs2.size() (2) Report error about size mismatch
  // ACTUAL: Preserves bs1's size but only processes min(blocks)
  //         Bits 50-99 in bs1 are unchanged (should be cleared)

  EXPECT_FALSE(bs1.test(70))
      << "Bit 70 should be cleared (bs2 doesn't have it)";
}

// ============================================================================
// FAILING TEST #15: XOR with self doesn't clear all bits when sizes differ
// ============================================================================

TEST(DynamicBitsetTest, XorWithSelfDoesntClearWhenPartial) {
  ErrorGuard guard;

  Bitset bs1(100, false);
  bs1.set(10);
  bs1.set(70);
  EXPECT_EQ(bs1.count(), 2);

  // XOR with self should clear everything
  bs1 ^= bs1;

  EXPECT_EQ(bs1.count(), 0) << "XOR with self should clear all bits";
  EXPECT_TRUE(bs1.none());

  // EXPECTED: All bits cleared
  // ACTUAL: This should work since both operands are the same object
  //         But if operator^= has a bug with min_blocks, it could fail
}

// ============================================================================
// PERFORMANCE TEST #1: O(n²) behavior in repeated push_back
// ============================================================================

TEST(DynamicBitsetPerf, RepeatedPushBackQuadraticBehavior) {
  ErrorGuard guard;

  // push_back calls resize(num_bits_ + 1) which may cause repeated
  // reallocations If blocks_ vector doesn't have sufficient capacity, this
  // becomes O(n²)

  Bitset bs;

  auto start = std::chrono::high_resolution_clock::now();

  // Push 10000 bits
  for (size_t i = 0; i < 10000; ++i) {
    bs.push_back(i % 2);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(bs.size(), 10000);
  EXPECT_EQ(bs.count(), 5000);

  // EXPECTED: Should complete in O(n) time with vector amortization
  // ACTUAL: May be slower than expected if resize() doesn't benefit from
  // vector::reserve()
  //         Baseline: should complete in <100ms on modern hardware

  std::cout << "10000 push_back operations took " << duration.count() << "ms"
            << std::endl;

  // This isn't a hard failure, but documents performance characteristics
  EXPECT_LT(duration.count(), 500)
      << "push_back seems slow - possible quadratic behavior";
}

// ============================================================================
// PERFORMANCE TEST #2: count() scans all blocks even if mostly empty
// ============================================================================

TEST(DynamicBitsetPerf, CountScansAllBlocks) {
  ErrorGuard guard;

  // Create huge bitset with only one bit set
  Bitset bs(1000000, false); // 15625 blocks
  bs.set(999999);

  auto start = std::chrono::high_resolution_clock::now();

  // Call count() many times
  size_t total = 0;
  for (int i = 0; i < 1000; ++i) {
    total += bs.count();
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  EXPECT_EQ(total, 1000);

  // EXPECTED: count() must scan all blocks - no early exit optimization
  // ACTUAL: count() is O(num_blocks) always, even if most blocks are empty
  //         This is acceptable but worth documenting

  std::cout << "1000 count() calls on 1M-bit sparse bitset took "
            << duration.count() << "us (" << (duration.count() / 1000.0)
            << "us per call)" << std::endl;

  // For 15625 blocks, this should still be fast (<10us per call)
  EXPECT_LT(duration.count() / 1000.0, 100.0)
      << "count() is slower than expected";
}

// ============================================================================
// PERFORMANCE TEST #3: Pathological resize() pattern
// ============================================================================

TEST(DynamicBitsetPerf, PathologicalResizePattern) {
  ErrorGuard guard;

  Bitset bs(1000, true);

  auto start = std::chrono::high_resolution_clock::now();

  // Alternate between growing and shrinking
  for (int i = 0; i < 1000; ++i) {
    bs.resize(1000 + (i % 100)); // Resize from 1000 to 1099 repeatedly
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  std::cout << "1000 alternating resize operations took " << duration.count()
            << "us" << std::endl;

  // EXPECTED: Should be fast since we're staying within same block count most
  // of the time ACTUAL: May trigger vector reallocations unnecessarily

  EXPECT_LT(duration.count(), 10000) << "resize() pattern is unexpectedly slow";
}

// ============================================================================
// PERFORMANCE TEST #4: Memory usage of sparse bitsets
// ============================================================================

TEST(DynamicBitsetPerf, SparseMemoryUsage) {
  ErrorGuard guard;

  // Create very large bitset with few bits set
  // Each block is 8 bytes, so 1M bits = 125000 blocks = 1MB minimum
  Bitset bs(1000000, false);
  bs.set(0);
  bs.set(999999);

  EXPECT_EQ(bs.count(), 2);

  // DIAGNOSIS: dynamic_bitset always allocates full storage
  // No sparse representation - 1M bits always costs ~1MB even if only 2 bits
  // set This is a design limitation, not a bug, but worth documenting

  // For truly sparse bitsets, a different data structure would be better
  // (e.g., hash set of set bits, or compressed blocks)

  std::cout << "1M-bit bitset with 2 bits set still uses "
            << (bs.size() / 8 / 1024) << " KB minimum" << std::endl;
}

// ============================================================================
// EDGE CASE TEST #1: Empty bitset operations
// ============================================================================

TEST(DynamicBitsetEdge, EmptyBitsetOperations) {
  ErrorGuard guard;

  Bitset bs;

  EXPECT_EQ(bs.size(), 0);
  EXPECT_TRUE(bs.empty());
  EXPECT_EQ(bs.count(), 0);
  EXPECT_TRUE(bs.all()); // Vacuous truth
  EXPECT_FALSE(bs.any());
  EXPECT_TRUE(bs.none());

  // Operations on empty bitset should be no-ops
  bs.set();
  EXPECT_EQ(bs.count(), 0);

  bs.reset();
  EXPECT_EQ(bs.count(), 0);

  bs.flip();
  EXPECT_EQ(bs.count(), 0);

  // Bitwise ops with empty
  Bitset bs2(10, false);
  bs2 &= bs;
  EXPECT_EQ(bs2.size(), 10);

  // Clear should work
  bs.clear();
  EXPECT_TRUE(bs.empty());
}

// ============================================================================
// EDGE CASE TEST #2: Single bit bitset
// ============================================================================

TEST(DynamicBitsetEdge, SingleBitBitset) {
  ErrorGuard guard;

  Bitset bs(1, false);

  EXPECT_EQ(bs.size(), 1);
  EXPECT_FALSE(bs.test(0));
  EXPECT_EQ(bs.count(), 0);
  EXPECT_FALSE(bs.all());
  EXPECT_FALSE(bs.any());
  EXPECT_TRUE(bs.none());

  bs.set(0);
  EXPECT_TRUE(bs.test(0));
  EXPECT_EQ(bs.count(), 1);
  EXPECT_TRUE(bs.all());
  EXPECT_TRUE(bs.any());
  EXPECT_FALSE(bs.none());

  bs.flip(0);
  EXPECT_FALSE(bs.test(0));
  EXPECT_TRUE(bs.none());

  // Out of range
  bs.set(1);
  EXPECT_TRUE(guard.has_errors());
}

// ============================================================================
// EDGE CASE TEST #3: Exactly 64-bit bitset
// ============================================================================

TEST(DynamicBitsetEdge, Exactly64Bits) {
  ErrorGuard guard;

  Bitset bs(64, false);

  EXPECT_EQ(bs.size(), 64);

  // Set all bits
  bs.set();
  EXPECT_EQ(bs.count(), 64);
  EXPECT_TRUE(bs.all());

  // Check boundaries
  EXPECT_TRUE(bs.test(0));
  EXPECT_TRUE(bs.test(63));

  // Clear one bit
  bs.reset(63);
  EXPECT_FALSE(bs.all());
  EXPECT_EQ(bs.count(), 63);

  // Flip all
  bs.flip();
  EXPECT_EQ(bs.count(), 1);
  EXPECT_TRUE(bs.test(63));
}

// ============================================================================
// EDGE CASE TEST #4: Bitset with 65 bits (one bit in second block)
// ============================================================================

TEST(DynamicBitsetEdge, OnebitSecondBlock) {
  ErrorGuard guard;

  Bitset bs(65, false);

  // Set only bit 64 (first bit of block 1)
  bs.set(64);

  EXPECT_EQ(bs.count(), 1);
  EXPECT_FALSE(bs.all());
  EXPECT_TRUE(bs.any());
  EXPECT_FALSE(bs.none());

  // all() should check partial blocks correctly
  EXPECT_FALSE(bs.all()) << "Not all bits are set";

  // Set all bits
  bs.set();
  EXPECT_TRUE(bs.all());
  EXPECT_EQ(bs.count(), 65);

  // Verify bit 64 specifically
  EXPECT_TRUE(bs.test(64));
}

// ============================================================================
// POLICY TEST: Small size_type (uint32_t)
// ============================================================================

TEST(DynamicBitsetPolicy, SmallSizeType) {
  core::ErrorCollector::instance().clear();

  SmallBitset bs(100, false);

  EXPECT_EQ(bs.size(), 100);

  bs.set(50);
  EXPECT_TRUE(bs.test(50));
  EXPECT_EQ(bs.count(), 1);

  // Test with max value for uint32_t (if feasible)
  // Note: Creating a bitset with 2^32 bits would require 512MB memory
  // This test just verifies the type works
}

// ============================================================================
// THREAD SAFETY TEST: Concurrent reads should be safe
// ============================================================================

TEST(DynamicBitsetThread, ConcurrentReads) {
  ErrorGuard guard;

  Bitset bs(1000, false);
  for (size_t i = 0; i < 1000; i += 2) {
    bs.set(i);
  }

  EXPECT_EQ(bs.count(), 500);

  // Launch multiple reader threads
  std::vector<std::thread> threads;
  std::atomic<size_t> total_count{0};

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&bs, &total_count]() {
      for (int j = 0; j < 100; ++j) {
        total_count += bs.count();
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(total_count, 500 * 1000);

  // EXPECTED: Concurrent reads should be safe
  // ACTUAL: const operations don't modify state, so no data races
  //         However, error reporting uses global ErrorCollector with mutex
}

// ============================================================================
// THREAD SAFETY TEST: Concurrent writes are NOT safe (data races)
// ============================================================================

TEST(DynamicBitsetThread, ConcurrentWritesDataRace) {
  // This test is DISABLED by default because it has intentional data races
  // Uncomment to verify that concurrent writes cause problems

  // ErrorGuard guard;
  // Bitset bs(1000, false);

  // std::vector<std::thread> threads;

  // for (int i = 0; i < 10; ++i) {
  //   threads.emplace_back([&bs, i]() {
  //     for (int j = 0; j < 100; ++j) {
  //       bs.set(i * 100 + j);
  //     }
  //   });
  // }

  // for (auto& t : threads) {
  //   t.join();
  // }

  // EXPECTED: Data races - undefined behavior
  // ACTUAL: set() modifies blocks_[i] without synchronization
  //         Multiple threads setting bits in the same block will race
  //         Even setting different bits in same block is a data race on the
  //         uint64_t

  // EXPECT_EQ(bs.count(), 1000) << "May not equal 1000 due to data races";
}

// ============================================================================
// COMPARISON TEST: Test that bitsets compare correctly after operations
// ============================================================================

TEST(DynamicBitsetComparison, BitwiseOpsEquivalence) {
  ErrorGuard guard;

  Bitset bs1(100, false);
  Bitset bs2(100, false);

  // Set same pattern in both
  for (size_t i = 0; i < 100; i += 3) {
    bs1.set(i);
    bs2.set(i);
  }

  // They should be equivalent
  EXPECT_EQ(bs1.count(), bs2.count());

  // Test each bit
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_EQ(bs1.test(i), bs2.test(i)) << "Bit " << i << " differs";
  }

  // XOR should give all zeros
  Bitset bs3 = bs1 ^ bs2;
  EXPECT_TRUE(bs3.none());
}

// ============================================================================
// COPY AND MOVE SEMANTICS TEST
// ============================================================================

TEST(DynamicBitsetCopyMove, CopyConstructor) {
  ErrorGuard guard;

  Bitset bs1(100, false);
  bs1.set(50);
  bs1.set(75);

  Bitset bs2(bs1);

  EXPECT_EQ(bs2.size(), 100);
  EXPECT_EQ(bs2.count(), 2);
  EXPECT_TRUE(bs2.test(50));
  EXPECT_TRUE(bs2.test(75));

  // Modify bs1 shouldn't affect bs2
  bs1.set(25);
  EXPECT_FALSE(bs2.test(25));
  EXPECT_EQ(bs1.count(), 3);
  EXPECT_EQ(bs2.count(), 2);
}

TEST(DynamicBitsetCopyMove, MoveConstructor) {
  ErrorGuard guard;

  Bitset bs1(100, false);
  bs1.set(50);
  bs1.set(75);

  Bitset bs2(std::move(bs1));

  EXPECT_EQ(bs2.size(), 100);
  EXPECT_EQ(bs2.count(), 2);
  EXPECT_TRUE(bs2.test(50));
  EXPECT_TRUE(bs2.test(75));

  // bs1 is in moved-from state (valid but unspecified)
  // We can still call operations on it
}

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
