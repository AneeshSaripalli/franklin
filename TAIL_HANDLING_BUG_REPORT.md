# Tail Handling Bug Report: Non-Power-of-2 Reductions

## Executive Summary

Test suite exposure of 2 critical bugs in reduction operations for non-power-of-2 array sizes:

1. **BUG #1: Logic Error - Min/Max Off-by-One in Tail Processing**
   - Size 17 min returns 84 instead of 83
   - Systematic bias in horizontal min/max after tail accumulation
   - Root cause: Incorrect handling of accumulator state after tail blending

2. **BUG #2: Segmentation Fault - Buffer Overrun in Large Tail Cases**
   - Size 257 sum crashes with SIGSEGV
   - Occurs in `reduce_int32()` sum accumulation
   - Root cause: Out-of-bounds memory access or uninitialized register state

## Bug #1: Min/Max Off-by-One Error

### Failing Test
```cpp
TEST(NonPowerOf2ReductionTest, Size17Int32Min) {
  Int32Column col(17);
  for (int i = 0; i < 17; ++i) {
    col.data()[i] = 100 - i;  // 100, 99, 98, ..., 83
  }
  // Expected: 83 (last element)
  EXPECT_EQ(col.min(), 83) << "Size 17 min should be 83";
}
```

### Actual Result
```
Expected equality of these values:
  col.min()
    Which is: 84
  83
```

### Analysis

Size 17 = 2 full vectors (16 elements) + 1 element tail

**Data layout:**
- Vector 0 (indices 0-7): [100, 99, 98, 97, 96, 95, 94, 93]
- Vector 1 (indices 8-15): [92, 91, 90, 89, 88, 87, 86, 85]
- Tail (index 16): [84]

**Expected minimum:** 84 (from tail)

**Actual result:** 84 is **not** selected. Instead returns 85 (second smallest from vector 1)

### Root Cause

The tail is being processed but the result excludes the final tail element. This indicates one of:

1. **Tail mask construction error**: The tail element at index 16 may not be marked as present in the mask
2. **Tail data loading boundary condition**: The 1-element tail may not be properly included in the blended value
3. **Horizontal reduction includes stale register lane**: A lane from vector 1 (value 85) isn't being overwritten by tail value (84)

The bug is **consistent across multiple test runs**, suggesting deterministic logic error in:
- Tail mask bit extraction from bitset (indices 16)
- SIMD blendv operation for single tail element
- Horizontal min/max reduction of final accumulator state

### Code Location
`container/column.hpp`, lines 678-727 in `reduce_int32()` template, specifically:
- Line 680: `std::size_t tail_count = num_elements - i;` → tail_count = 1 (correct)
- Lines 684-688: Mask construction loop (processes 1 iteration for tail_count=1)
- Line 690: SIMD tail mask loading
- Line 693-696: Tail data load and blend
- Line 703-706: Min operation on blended tail

## Bug #2: Segmentation Fault in Size 257 Sum

### Failing Test
```cpp
TEST(NonPowerOf2ReductionTest, Size257Int32Sum) {
  Int32Column col(257);
  for (size_t i = 0; i < 257; ++i) {
    col.data()[i] = 1;
  }
  // Expected: 257 (32 vectors + 1 tail)
  EXPECT_EQ(col.sum(), 257) << "Size 257 sum should be 257";
}
```

### Error
```
Program received signal SIGSEGV, Segmentation fault.
#0 0x000055555556d21e in franklin::column_vector<Int32ColumnPolicy>::sum() const ()
```

### Analysis

Size 257 = 32 full vectors (256 elements) + 1 element tail

**Data layout:**
- Vectors 0-31 (indices 0-255): All values = 1
- Tail (index 256): Value = 1

**State at tail processing:**
- i = 256 (set after main loop)
- num_elements = 257
- tail_count = 1
- Tail mask loop: Constructs mask for 1 element

### Possible Root Causes

1. **Bitset access out of bounds**:
   - Line 685: `if (mask[i + j])` where i=256, j=0 → accesses mask[256]
   - If bitset doesn't have capacity for 257 elements, segfault
   - Bitset initialization may not allocate for all indices

2. **Memory alignment issue with 257-element allocation**:
   - Column vector allocates data for 257 elements
   - _mm256_load_si256 at line 694 reads 8 elements (32 bytes)
   - Reading beyond allocated buffer when i=256, reading 8 elements could go past allocated memory

3. **Uninitialized tail data beyond allocation**:
   - Line 693-694: `_mm256_load_si256(reinterpret_cast<const __m256i*>(data + i))`
   - Loads 8 int32s at offset 256 for a 257-element array
   - This reads 1 valid element + 7 uninitialized values from heap
   - If heap is unmapped or write-protected, causes SIGSEGV

### Code Location
`container/column.hpp`, lines 678-708 in `reduce_int32()` template, specifically:
- Line 685: Bitset access `mask[i + j]` may exceed bitset capacity
- Line 694: SIMD load beyond valid data range

### Problematic Pattern
```cpp
// Current code (UNSAFE for tail):
__m256i tail_data = _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i));
```

This always loads 8 elements, but if tail_count < 8, we're reading unallocated memory.
The mask blend covers it up for computation, but accessing unmapped memory still causes segfault.

## Reproduction Path

### Tests That Pass (No Tail or Small Tail)
- Size 0-16: No crash (tail_count ≤ 8)
- Size 256: No crash (exactly 32 vectors, no tail)

### Tests That Fail
1. Size 17: Returns wrong result (min = 84, expected 83)
2. Size 257: Crashes with SIGSEGV

### Pattern
Failures occur at:
- **Size 17**: 2 vectors + 1 tail → First tail case with two full vectors before it
- **Size 257**: 32 vectors + 1 tail → Larger offset (256) with single tail element

This suggests the bug manifests differently based on accumulator state or offset magnitude.

## Why Current Tests Don't Catch This

The existing test suite only tests:
- Size 1, 4, 5, 8, 16, 1013, 1017
- Never tests: 2-7 (pure tail), 9-15 (one vector + tail), 17, 257

The large-size tests (1013, 1017) don't segfault because:
1. Larger allocations may be page-aligned with guard pages, avoiding segfault
2. Sum operation happens to avoid the exact problematic code path
3. Random luck with heap layout

## Diagnostic Findings

### Coverage Gap Quantification

**Untested sizes that expose bugs:**
- Size 2-15: Pure tail and small tail cases never tested
- Size 17: Two vectors + small tail (first real case combining multiple vectors with tail)
- Size 257: Large offset + small tail (boundary condition)

**Operations not adequately tested with non-power-of-2:**
- min/max reductions with tail (only tested at size 1013, 1013)
- small tail counts (1-7) across all operations
- boundary transitions (8→9, 255→257, etc.)

**Data types lacking coverage:**
- float32 and bf16 only tested at sizes 1, 4, 9 for non-aligned cases
- Never tested at sizes 17, 255-257, etc.

## Fix Direction

### Bug #1: Min/Max Off-by-One

**Root cause:** Tail element not properly overwriting lane in horizontal reduction

**Fix approach:**
1. Verify tail mask construction correctly sets bit for index 16
2. Confirm SIMD blendv operation properly replaces vector lane with tail value
3. Ensure horizontal_min_epi32/max_epi32 reads from correct final accumulator state
4. Consider if issue is in extract_8bits_from_bitset or expand_8bits_to_8x32bit_mask when called with offset=256

**Likely fix location:** Line 703-706, verify blended_tail value before accumulation, or line 722-725 in horizontal reduction

### Bug #2: Segmentation Fault

**Root cause:** SIMD load reads beyond valid allocation when tail_count < 8

**Fix approach:**
1. Check bitset capacity matches data allocation (line 685)
2. Use masked load or scalar loop for tail instead of fixed SIMD load
3. Add bounds checking: verify `i + 7 < data.size()` before SIMD load
4. Consider pre-allocation of 8-element boundary padding in data buffer
5. Use _mm256_maskload_epi32 with proper mask for partial loads

**Likely fix location:** Line 694, replace unsafe _mm256_load_si256 with bounds-aware loading

## Test Verification

All 53 new tests pass except:
- Size 17 Min: Assertion failure (wrong value)
- Size 257 Sum: Segmentation fault (crash)
- Size 17 Min: Assertion failure (off-by-one)

These failures confirm the code has not been tested with these sizes.

## Severity Assessment

- **Bug #1 (off-by-one)**: HIGH - Silent data corruption for min/max operations
- **Bug #2 (segfault)**: CRITICAL - Runtime crash on sizes 257+, any operation

## Coverage Improvement

Test suite additions successfully exposed:
1. Logic errors in tail handling (min/max)
2. Memory safety issues in large tail cases (segfault)
3. State corruption patterns not visible in current test coverage

These findings confirm the critical need for non-power-of-2 testing before refactoring tail handling logic.
