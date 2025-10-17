#include "container/dynamic_bitset.hpp"
#include "core/error_collector.hpp"
#include <gtest/gtest.h>

namespace franklin {

struct TestPolicy {
  using size_type = std::size_t;
};

using Bitset = dynamic_bitset<TestPolicy>;

// RAII helper for error management
class ErrorGuard {
public:
  ErrorGuard() {
    core::ErrorCollector::instance().clear();
    core::ErrorCollector::instance().set_enabled(true);
  }
  ~ErrorGuard() { core::ErrorCollector::instance().clear(); }
};

// ============================================================================
// ADVERSARIAL TEST #1: count() with non-power-of-2 size (size 7)
// ============================================================================

TEST(DynamicBitsetAdversarial, CountWithSize7AllSet) {
  ErrorGuard guard;

  // A) TEST CODE
  // Create bitset with 7 bits (non-power-of-2, less than 8)
  Bitset bs(7, true); // All bits should be set

  // Verify individual bits are set
  for (size_t i = 0; i < 7; ++i) {
    EXPECT_TRUE(bs.test(i)) << "Bit " << i << " should be set";
  }

  // The critical test: count() should return exactly 7
  size_t actual_count = bs.count();
  EXPECT_EQ(actual_count, 7) << "FAILURE: count() returned " << actual_count
                             << " but expected 7. Implementation may be "
                                "counting bits beyond num_bits_.";
}

// B) DIAGNOSIS
// The count() implementation at line 280 calls simd_count_blocks(data,
// last_block_idx) where last_block_idx = block_index(6) = 0. This correctly
// skips the SIMD loop. Then at line 287, it masks the last block: mask = (1 <<
// 7) - 1 = 0x7F However, if num_blocks_needed(7) rounds up to 8 blocks (64
// bytes), blocks_[0] might contain garbage in bits 7-63 that wasn't zeroed
// properly. If zero_unused_bits() wasn't called correctly, count will include
// those bits.
//
// Root cause: Mismatch between num_blocks_needed() rounding to 8-block
// boundaries and count() assuming only the bits within num_bits_ matter.

// C) FIX DIRECTION
// Ensure constructor with value=true calls zero_unused_bits() after setting
// blocks, OR change num_blocks() to return actual needed blocks, not rounded
// blocks.

// ============================================================================
// ADVERSARIAL TEST #2: all() with size 15 (not aligned to SIMD width)
// ============================================================================

TEST(DynamicBitsetAdversarial, AllWithSize15NotAlignedToSIMD) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 15: 15 bits across 1 block (block 0 contains bits 0-63)
  // SIMD loop processes blocks in chunks of 8
  Bitset bs(15, true); // Set all 15 bits

  // Verify each bit individually
  for (size_t i = 0; i < 15; ++i) {
    EXPECT_TRUE(bs.test(i)) << "Bit " << i << " should be set";
  }

  // all() should return true since all valid bits are set
  EXPECT_TRUE(bs.all())
      << "FAILURE: all() returned false but all 15 bits are set. "
      << "Implementation may be checking unused bits or has loop boundary "
         "error.";
}

// B) DIAGNOSIS
// all() at line 355 computes: simd_blocks = last_block_idx / 8 = 0 / 8 = 0
// So the SIMD loop doesn't run. Then at line 385, it checks remaining full
// blocks before last_block_idx. Since i=0 and last_block_idx=0, this loop
// doesn't run either. Finally at line 392, it checks if remainder == 0. For
// size 15, remainder = 15 % 64 = 15. So it masks: mask = (1 << 15) - 1 = 0x7FFF
// Returns: (data[0] & 0x7FFF) == 0x7FFF
//
// BUG: If blocks_[0] has bits 15-63 set due to set() not calling
// zero_unused_bits(), the check will fail because it's comparing the masked
// value against the mask, but it should mask BOTH sides of the comparison.
// Actually looking at line 398: return (data[last_block_idx] & mask) == mask;
// This is correct IF the bits 15-63 in data[0] are zero. If they're not, this
// still works because we're masking. The real bug is earlier.
//
// Actually the real issue: set() at line 196 says "Note: We don't call
// zero_unused_bits()" So bits beyond num_bits_ remain as all-ones. Then all()
// will see them and may fail.

// C) FIX DIRECTION
// set() must call zero_unused_bits() after setting all blocks to ensure
// invariant: "bits beyond num_bits_ are always zero". Without this, all
// operations that check bit patterns (all, count, etc) become incorrect.

// ============================================================================
// ADVERSARIAL TEST #3: count() with size 127 (just before 2-block boundary)
// ============================================================================

TEST(DynamicBitsetAdversarial, CountWithSize127OffByOne) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 127 = 64 + 63 = 2 blocks (block 0 full, block 1 has 63 bits)
  Bitset bs(127, false);

  // Set exactly 127 bits
  for (size_t i = 0; i < 127; ++i) {
    bs.set(i);
  }

  // Verify manually
  size_t manual_count = 0;
  for (size_t i = 0; i < 127; ++i) {
    if (bs.test(i)) {
      manual_count++;
    }
  }
  EXPECT_EQ(manual_count, 127) << "Manual count should be 127";

  // The critical test
  size_t actual_count = bs.count();
  EXPECT_EQ(actual_count, 127)
      << "FAILURE: count() returned " << actual_count << " but expected 127. "
      << "Off-by-one error in block boundary handling.";
}

// B) DIAGNOSIS
// count() computes: last_block_idx = block_index(126) = 126 >> 6 = 1
// remainder = 127 & 63 = 63
// Calls simd_count_blocks(data, 1) which counts block 0 only (simd_blocks = 1/4
// = 0, then loop at line 35 counts blocks 0..0). Then at line 289, it computes:
// mask = (1 << 63) - 1 = 0x7FFFFFFFFFFFFFFF Result = count_of_block_0 +
// popcnt(data[1] & mask)
//
// BUG: If num_blocks_needed(127) returns 8 (rounded up), blocks_.size() = 8.
// But we only care about blocks 0-1. The issue is if mask calculation is wrong.
// Specifically, if remainder = 63, mask = (1 << 63) - 1.
// WARNING: 1 << 63 on uint64_t is UB when 1 is int (left shift by 63 on int is
// UB). If block_type(1) is uint64_t, then (uint64_t(1) << 63) is defined =
// 0x8000000000000000, and subtracting 1 gives 0x7FFFFFFFFFFFFFFF which is
// correct.

// C) FIX DIRECTION
// Check that block_type(1) is actually uint64_t (it is, see line 45), so this
// should work. The actual bug might be that simd_count_blocks is called with
// last_block_idx (1) but it should be called with last_block_idx (exclusive),
// or the final block handling is wrong. Need to verify the loop bounds at line
// 280.

// ============================================================================
// ADVERSARIAL TEST #4: all() with size 65 (one bit into second block)
// ============================================================================

TEST(DynamicBitsetAdversarial, AllWithSize65OnebitSecondBlock) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 65 = 64 + 1 (block 0 full, block 1 has 1 bit)
  Bitset bs(65, true); // All 65 bits set

  // Verify the critical bit
  EXPECT_TRUE(bs.test(64)) << "Bit 64 (first of block 1) should be set";

  // all() should return true
  EXPECT_TRUE(bs.all())
      << "FAILURE: all() returned false but all 65 bits are set. "
      << "Partial block in second block not handled correctly.";
}

// B) DIAGNOSIS
// all() computes: last_block_idx = block_index(64) = 1
// remainder = 65 & 63 = 1
// simd_blocks = 1 / 8 = 0 (SIMD loop doesn't run)
// Loop at line 385: for i in [0, 1): check if data[0] == ~0. This should pass.
// Then at line 397: mask = (1 << 1) - 1 = 1
// Returns: (data[1] & 1) == 1
//
// BUG: If set() didn't call zero_unused_bits(), data[1] might have all 64 bits
// set. Then data[1] & 1 = 1, so the check passes. Actually this case works.
//
// Real bug is different: If blocks_.size() = 8 (rounded), but we only
// initialized blocks 0-1, blocks 2-7 have undefined values. However, all() only
// checks up to last_block_idx = 1, so this is fine.

// C) FIX DIRECTION
// Actually this test might pass. The real issue is when size = 71 (not multiple
// of 8).

// ============================================================================
// ADVERSARIAL TEST #5: all() with size 71 (misaligned to 8-block cache line)
// ============================================================================

TEST(DynamicBitsetAdversarial, AllWithSize71MisalignedToBlockBoundary) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 71 = 64 + 7 (block 0 full, block 1 has 7 bits)
  Bitset bs(71, false);

  // Set all 71 bits manually
  for (size_t i = 0; i < 71; ++i) {
    bs.set(i);
  }

  // Verify each bit
  for (size_t i = 0; i < 71; ++i) {
    EXPECT_TRUE(bs.test(i)) << "Bit " << i << " not set";
  }

  // all() should return true
  EXPECT_TRUE(bs.all())
      << "FAILURE: all() returned false but all 71 bits are set. "
      << "SIMD loop boundary handling is incorrect for non-8-aligned block "
         "counts.";
}

// B) DIAGNOSIS
// all() computes: last_block_idx = block_index(70) = 1
// simd_blocks = 1 / 8 = 0 (no SIMD)
// Loop at line 385: for i in [0, 1): check data[0] == ~0
// Then check data[1] with remainder = 7: mask = (1 << 7) - 1 = 0x7F
// Returns: (data[1] & 0x7F) == 0x7F
//
// This should work IF data[1] has bits 0-6 set and bits 7-63 are zero.
// The bug is if bits 7-63 in data[1] are non-zero from uninitialized memory
// or from set() not calling zero_unused_bits().
//
// Wait, set(i) is called for each i, so each bit is set individually.
// set(i) at line 172 does: blocks_[block_index(i)] |= bit_mask(i)
// This doesn't clear other bits, so if blocks_[1] started with garbage,
// bits 7-63 might be set. Then all() checks (data[1] & 0x7F) == 0x7F,
// which will be true if bits 0-6 are set, regardless of bits 7-63.

// C) FIX DIRECTION
// The real issue is in set() at line 179: bulk set() doesn't call
// zero_unused_bits() at the end (see line 199 comment). This violates the
// invariant that padding bits are always zero. Must call zero_unused_bits()
// after the SIMD loop.

// ============================================================================
// ADVERSARIAL TEST #6: Bitwise AND with size 13 (prime, under SIMD width)
// ============================================================================

TEST(DynamicBitsetAdversarial, BitwiseAndSize13LoopBoundary) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 13 (prime, less than 16)
  Bitset bs1(13, false);
  Bitset bs2(13, false);

  // Set alternating pattern
  for (size_t i = 0; i < 13; i += 2) {
    bs1.set(i); // 1010101010101 (bits 0,2,4,6,8,10,12)
  }
  for (size_t i = 1; i < 13; i += 2) {
    bs2.set(i); // 0101010101010 (bits 1,3,5,7,9,11)
  }

  EXPECT_EQ(bs1.count(), 7);
  EXPECT_EQ(bs2.count(), 6);

  // AND should produce all zeros
  bs1 &= bs2;

  EXPECT_EQ(bs1.count(), 0)
      << "FAILURE: AND of disjoint patterns should be 0, but got "
      << bs1.count() << ". "
      << "SIMD loop with i+=8 doesn't handle small sizes correctly.";

  // Verify each bit
  for (size_t i = 0; i < 13; ++i) {
    EXPECT_FALSE(bs1.test(i)) << "Bit " << i << " should be 0 after AND";
  }
}

// B) DIAGNOSIS
// operator&= at line 413: min_blocks = min(blocks_.size(),
// other.blocks_.size()) num_blocks_needed(13) returns (13 + 63) >> 6 = 76 >> 6
// = 1, then (1 + 7) & ~7 = 8. So blocks_.size() = 8 for both. min_blocks = 8.
// Loop at line 413: for (i = 0; i < 8; i += 8) runs ONCE for i=0..7.
// Loads blocks 0-3 and 4-7, performs AND, stores back.
//
// BUG: For size 13, only block 0 contains valid data (bits 0-12).
// Blocks 1-7 are padding and should be zero. The AND operation will AND garbage
// with garbage in blocks 1-7, potentially leaving non-zero values there. Then
// when we check count() or test(), we only check bits 0-12, so garbage doesn't
// affect us.
//
// Wait, actually the implementation seems sound IF blocks 1-7 are zeroed during
// construction. Constructor calls num_blocks_needed(13) = 8, then resizes
// blocks_ to 8 with value=false, so blocks 0-7 are all zero initially. Then
// setting bits only affects block 0. Then operator&= ANDs all 8 blocks, which
// is correct.

// C) FIX DIRECTION
// This test should actually pass. The rounding to 8 blocks is intentional to
// avoid tail handling in SIMD loops. Need to find a different edge case.

// ============================================================================
// ADVERSARIAL TEST #7: Bitwise AND with size 17 (just over 2 SIMD iterations)
// ============================================================================

TEST(DynamicBitsetAdversarial, BitwiseAndSize17TailHandling) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 17: Requires 2 AVX2 registers (8 elements each), with 1 element tail
  Bitset bs1(17, true);  // All ones
  Bitset bs2(17, false); // All zeros
  bs2.set(16);           // Set last bit only

  EXPECT_EQ(bs1.count(), 17);
  EXPECT_EQ(bs2.count(), 1);

  bs1 &= bs2;

  // Should have only bit 16 set
  EXPECT_EQ(bs1.count(), 1)
      << "FAILURE: AND should leave only bit 16 set, but count = "
      << bs1.count();

  EXPECT_TRUE(bs1.test(16)) << "Bit 16 should be set";
  for (size_t i = 0; i < 16; ++i) {
    EXPECT_FALSE(bs1.test(i)) << "Bit " << i << " should be clear";
  }
}

// B) DIAGNOSIS
// num_blocks_needed(17) = ((17 + 63) >> 6 = 1), then (1 + 7) & ~7 = 8 blocks.
// operator&= processes all 8 blocks in one iteration of i+=8.
// Block 0 contains bits 0-16 (17 bits).
// Block 0 before: bs1 has all ones in bits 0-16, bs2 has only bit 16 set.
// After AND: block 0 should have only bit 16 set.
// This should work correctly.
//
// The test verifies correctness, not failure. Need to think harder about actual
// bugs.

// C) FIX DIRECTION
// Re-examine the implementations for actual boundary bugs.

// ============================================================================
// ADVERSARIAL TEST #8: flip() with size 67 leaves garbage in padding
// ============================================================================

TEST(DynamicBitsetAdversarial, FlipSize67LeavesGarbageInPadding) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 67 = 64 + 3 (block 0 full, block 1 has 3 bits)
  Bitset bs(67, false); // All zeros

  EXPECT_EQ(bs.count(), 0);

  // Flip all bits
  bs.flip();

  // Should have exactly 67 bits set
  size_t actual_count = bs.count();
  EXPECT_EQ(actual_count, 67)
      << "FAILURE: flip() on 67-bit bitset should set exactly 67 bits, but got "
      << actual_count
      << ". Padding bits 3-63 in block 1 may be flipped incorrectly.";

  // Verify each bit individually
  for (size_t i = 0; i < 67; ++i) {
    EXPECT_TRUE(bs.test(i)) << "Bit " << i << " should be set after flip()";
  }
}

// B) DIAGNOSIS
// flip() at line 230: Processes num_blocks = blocks_.size() blocks.
// num_blocks_needed(67) = (67+63)>>6 = 2, then (2+7)&~7 = 8 blocks.
// Loop at line 243: for (i = 0; i < 8; i += 8) runs once, flipping all 8
// blocks. Blocks 0-1 contain data, blocks 2-7 are padding.
//
// After flip:
// - Block 0: all bits flipped (now all ones) ✓
// - Block 1: all bits flipped (now all ones) ✗ Should only flip bits 0-2!
// - Blocks 2-7: all bits flipped (now all ones) ✗ Should remain zero!
//
// Then count() computes:
// - last_block_idx = block_index(66) = 1
// - Calls simd_count_blocks(data, 1) which counts block 0 = 64
// - remainder = 67 & 63 = 3
// - mask = (1 << 3) - 1 = 7
// - Counts bits in data[1] & 7 = 3 bits set
// - Total = 64 + 3 = 67 ✓
//
// So count() MASKS the padding bits correctly, hiding the bug!
// But the internal state is wrong: blocks 1-7 have garbage.
//
// CRITICAL BUG FOUND: flip() doesn't call zero_unused_bits() at the end (line
// 256). This violates the invariant and can cause issues with operations that
// don't mask.

// C) FIX DIRECTION
// flip() must call zero_unused_bits() after the SIMD loop to clear padding
// bits. Without this, the bitset is in an invalid state even though count()
// hides it.

// ============================================================================
// ADVERSARIAL TEST #9: any() with size 31 (prime) all zeros except last bit
// ============================================================================

TEST(DynamicBitsetAdversarial, AnySize31OnlyLastBitSet) {
  ErrorGuard guard;

  // A) TEST CODE
  // Size 31 (prime, less than 64)
  Bitset bs(31, false);

  // Set only the last bit
  bs.set(30);

  EXPECT_EQ(bs.count(), 1);
  EXPECT_TRUE(bs.test(30));

  // any() should detect the single set bit
  EXPECT_TRUE(bs.any()) << "FAILURE: any() returned false but bit 30 is set. "
                        << "SIMD loop or tail handling in any() is incorrect.";
}

// B) DIAGNOSIS
// any() at line 298: num_blocks_total = blocks_.size() = 8 (rounded)
// simd_blocks = 8 / 8 = 1
// Loop at line 312: Loads blocks 0-7, ORs them in pairs, checks if any bits
// set. Block 0 has bit 30 set, so combined result has bit 30 set. testz returns
// false, so any() returns true early at line 324. ✓
//
// This should work correctly. The SIMD loop checks all blocks efficiently.

// C) FIX DIRECTION
// This test should pass. Need to find actual failing case.

// ============================================================================
// ADVERSARIAL TEST #10: operator&= with mismatched sizes (13 vs 67)
// ============================================================================

TEST(DynamicBitsetAdversarial, BitwiseAndMismatchedSizes13And67) {
  ErrorGuard guard;

  // A) TEST CODE
  Bitset bs1(67, true); // All 67 bits set
  Bitset bs2(13, true); // All 13 bits set

  EXPECT_EQ(bs1.count(), 67);
  EXPECT_EQ(bs2.count(), 13);

  // AND bs1 with bs2 (different sizes)
  bs1 &= bs2;

  // Expected: bits 0-12 remain set (intersection), bits 13-66 cleared (bs2
  // implicitly 0) Actual: Implementation uses min_blocks, so bits beyond block
  // 0 might not be cleared

  size_t actual_count = bs1.count();
  EXPECT_EQ(actual_count, 13)
      << "FAILURE: After AND with smaller bitset, should have 13 bits set, but "
         "got "
      << actual_count << ". Bits beyond smaller operand's size not cleared.";

  // Verify bits 0-12 are set
  for (size_t i = 0; i < 13; ++i) {
    EXPECT_TRUE(bs1.test(i)) << "Bit " << i << " should be set";
  }

  // Verify bits 13-66 are clear
  for (size_t i = 13; i < 67; ++i) {
    EXPECT_FALSE(bs1.test(i)) << "Bit " << i << " should be clear after AND";
  }
}

// B) DIAGNOSIS
// operator&= at line 402:
// min_blocks = min(8, 8) = 8 (both have num_blocks_needed = 8)
// Wait, num_blocks_needed(13) = (13+63)>>6 = 1, then (1+7)&~7 = 8.
// num_blocks_needed(67) = (67+63)>>6 = 2, then (2+7)&~7 = 8.
// So both have 8 blocks, min_blocks = 8.
//
// Loop processes all 8 blocks. bs2 has data only in block 0, blocks 1-7 are
// zero. bs1 & bs2 will clear blocks 1-7 in bs1. ✓
//
// Then at line 431: remaining_blocks = 8 - 8 = 0, so no memset.
//
// After AND:
// - Block 0: bs1[0] & bs2[0] = bits 0-66 & bits 0-12 = bits 0-12 set ✓
// - Blocks 1-7: bs1[i] & bs2[i] = something & 0 = 0 ✓
//
// count() will count block 0 (with bits 0-12 set) and mask block 1 (3 bits of
// block 1). Result = 13. ✓
//
// This should actually work correctly because of the 8-block rounding!

// C) FIX DIRECTION
// The rounding to 8 blocks makes this work. Need to test truly mismatched
// cases.

// ============================================================================
// ADVERSARIAL TEST #11: resize from 7 to 71 with value=true
// ============================================================================

TEST(DynamicBitsetAdversarial, ResizeFrom7To71WithValueTrue) {
  ErrorGuard guard;

  // A) TEST CODE
  Bitset bs(7, false);
  bs.set(3); // Set one bit

  EXPECT_EQ(bs.count(), 1);

  // Resize to 71 with value=true (new bits should be set)
  bs.resize(71, true);

  EXPECT_EQ(bs.size(), 71);

  // Bit 3 should still be set
  EXPECT_TRUE(bs.test(3)) << "Original bit 3 should be preserved";

  // Bits 7-70 should be set (new bits)
  for (size_t i = 7; i < 71; ++i) {
    EXPECT_TRUE(bs.test(i))
        << "FAILURE: New bit " << i
        << " should be set after resize with value=true, but it's not. "
        << "resize() doesn't properly set new bits in expanded range.";
  }

  // Count should be 1 + 64 = 65
  size_t actual_count = bs.count();
  EXPECT_EQ(actual_count, 65)
      << "FAILURE: Should have 65 bits set (bit 3 + bits 7-70), but got "
      << actual_count;
}

// B) DIAGNOSIS
// resize() at line 101:
// old_num_bits = 7, old_num_blocks = num_blocks() = (7+63)>>6 = 1
// num_bits_ = 71, new_num_blocks = num_blocks() = (71+63)>>6 = 2
//
// At line 107: new_num_blocks (2) != old_num_blocks (1), so calls
// blocks_.resize(2, ~0) This sets block 1 to all-ones. ✓
//
// At line 112-118: num_bits (71) > old_num_bits (7) && value (true), so:
// Loop from i=7 to 70, setting each bit via blocks_[block_index(i)] |=
// bit_mask(i) This sets bits 7-63 in block 0, and bits 64-70 in block 1. ✓
//
// At line 123-125: Since value is true, calls zero_unused_bits().
// remainder = 71 & 63 = 7
// mask = (1 << 7) - 1 = 0x7F
// blocks_.back() (block 1) &= 0x7F, clearing bits 7-63. ✓
//
// Result:
// - Block 0: bit 3 + bits 7-63 set = 58 bits
// - Block 1: bits 64-70 set = 7 bits
// Total = 65 bits ✓
//
// Wait, this should actually work! Let me recalculate num_blocks().

// Actually, line 72 num_blocks() uses: (num_bits_ + bits_per_block - 1) >>
// block_shift Which is the actual needed blocks, NOT the rounded-up value. But
// line 513 num_blocks_needed() uses: ((blocks + 7) & ~7) for rounding.
//
// AH! There are TWO different functions:
// - num_blocks() at line 72: returns actual needed blocks (not rounded)
// - num_blocks_needed() at line 513: returns rounded-up blocks (multiple of 8)
//
// Constructor uses num_blocks_needed() (line 89), but resize() uses
// num_blocks() (line 103)!
//
// CRITICAL BUG: Inconsistent use of num_blocks() vs num_blocks_needed()!

// C) FIX DIRECTION
// resize() should use num_blocks_needed() instead of num_blocks() to maintain
// consistency. Or all operations should use num_blocks() and remove
// num_blocks_needed().

// ============================================================================
// ADVERSARIAL TEST #12: set() bulk operation with size 23 (prime)
// ============================================================================

TEST(DynamicBitsetAdversarial, BulkSetSize23LeavesGarbage) {
  ErrorGuard guard;

  // A) TEST CODE
  Bitset bs(23, false);

  EXPECT_EQ(bs.count(), 0);

  // Bulk set all bits
  bs.set();

  // Should have exactly 23 bits set
  size_t actual_count = bs.count();
  EXPECT_EQ(actual_count, 23)
      << "FAILURE: set() should set exactly 23 bits, but got " << actual_count
      << ". "
      << "Bits 23-63 in block 0 may be set incorrectly, leaving garbage.";

  // Verify individual bits
  for (size_t i = 0; i < 23; ++i) {
    EXPECT_TRUE(bs.test(i)) << "Bit " << i << " should be set";
  }
}

// B) DIAGNOSIS
// set() at line 179: blocks_.size() = num_blocks_needed(23) = 8
// Loop at line 192: for (i = 0; i < 8; i += 8) runs once, setting blocks 0-7 to
// all-ones. Block 0 now has all 64 bits set (should only have bits 0-22).
// Blocks 1-7 now have all bits set (should be zero).
//
// Line 199 comment: "Note: We don't call zero_unused_bits() because padding
// bits are masked off in summary operations (count(), all(), any(), none())"
//
// This is WRONG! The invariant should be that padding bits are ALWAYS zero.
// Otherwise, operations that don't mask will see garbage.
//
// count() at line 280 does mask: remainder = 23, mask = (1 << 23) - 1 =
// 0x7FFFFF Counts data[0] & 0x7FFFFF = 23 bits. ✓
//
// But internal state is invalid: bits 23-63 in block 0 are all-ones.
// If we later resize or perform other operations, this garbage can propagate.

// C) FIX DIRECTION
// set() must call zero_unused_bits() after the SIMD loop to maintain invariant.
// The comment at line 199 is a dangerous optimization that breaks invariants.

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
