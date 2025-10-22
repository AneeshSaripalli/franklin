# Critical Test Cases for Tail Handling in Reductions

## Overview

This document details the 53 test cases added to expose tail-handling logic errors in reduction operations. Each test targets a specific code path or boundary condition.

## Test Case Categories

### Category 1: Pure Tail Cases (No Full Vectors)

These test the case where `num_elements < 8`, so the main loop never executes. The accumulator initialization and tail blending are critical.

#### Test: Size2Int32Sum
**Size**: 2 elements
**Operation**: Sum
**Data**: [5, 7]
**Expected**: 12
**Why it matters**:
- Main loop never executes (num_full_vecs = 0)
- Entirely relies on tail processing
- Exposes: Uninitialized accumulator at identity (0), tail blending
**Code path**: Lines 679-708 only (no line 653-676)

#### Test: Size3Int32Product
**Size**: 3 elements
**Operation**: Product
**Data**: [2, 3, 4]
**Expected**: 24
**Why it matters**:
- Product identity = 1, must not multiply by uninitialized value
- First test where initial accumulator state matters
- Exposes: Identity vector initialization correctness
**Code path**: Lines 646-647, 679-708

#### Test: Size5Int32Sum (Baseline from existing)
**Size**: 5 elements
**Operation**: Sum
**Data**: [1, 2, 3, 4, 5]
**Expected**: 15
**Why it matters**:
- Odd size tests mask boundary conditions
- Already in current suite, but critical
**Status**: Already passing

#### Test: Size7Int32Product
**Size**: 7 elements (maximum pure tail)
**Operation**: Product
**Data**: [1, 2, 2, 1, 3, 1, 2]
**Expected**: 24
**Why it matters**:
- Maximum tail without full vector
- 7 is binary 0111 (all low bits set) - tests mask extraction at position 7
- Last element in potential 8-bit mask
**Code path**: Tail mask construction with tail_count=7 (line 684)

#### Test: Size4MinTailFirstElementMasked
**Size**: 4 elements
**Operation**: Min
**Data**: [10, 50, 75, 25]
**Mask**: Element 0 masked
**Expected**: 25
**Why it matters**:
- Tests identity blending for masked elements (INT_MAX for min)
- Small tail with selective masking
- Exposes: Tail mask array construction at specific index
**Code path**: Lines 684-690 with j in range [0, 4)

### Category 2: One Vector + Tail (8 < size < 16)

These test the transition from main loop to tail, ensuring accumulator state carries over correctly.

#### Test: Size9Int32Sum
**Size**: 9 elements
**Operation**: Sum
**Data**: [1, 2, ..., 9]
**Expected**: 45
**Why it matters**:
- First test with one full vector followed by tail
- Main loop executes exactly once, then tail
- Exposes: Accumulator carryover from loop to tail
**Code path**: Line 653-676 (1 iteration), then 679-708

#### Test: Size9ProductWithTailMasked
**Size**: 9 elements
**Operation**: Product
**Data**: All 2s
**Mask**: Tail element (index 8) masked
**Expected**: 256 (2^8)
**Why it matters**:
- Single tail element with mask = 0
- Tail blend must use identity (1.0) not data
- Exposes: Tail mask bit construction for single masked element
**Code path**: Line 685: `mask[8]` must be false

#### Test: Size15Int32Sum
**Size**: 15 elements
**Operation**: Sum
**Data**: All 1s
**Expected**: 15
**Why it matters**:
- Maximum partial tail (7 elements, binary 0111_1111)
- One vector + 7 tail
- Tests mask construction with tail_count=7
**Code path**: Tail loop iterations j=0 to j=6

### Category 3: Two Vectors + Tail (16 < size < 24)

These test accumulation across multiple full vectors with tail carryover.

#### Test: Size17Int32Min (FAILS - BUG #1)
**Size**: 17 elements
**Operation**: Min
**Data**: [100, 99, ..., 84] (descending)
**Expected**: 83 (from tail at index 16)
**Actual**: 84 (from vector 1)
**Why it matters**:
- **EXPOSES BUG**: Min result doesn't include tail minimum
- Two vectors processed in main loop, then tail
- Tail value should override accumulator lanes
**Code path**: Main loop (lines 653-676) × 2, then tail (679-708)
**Root cause hypothesis**: Tail blend doesn't properly replace all SIMD lanes with tail value

#### Test: Size17ProductFirstTailElementMasked
**Size**: 17 elements
**Operation**: Product
**Data**: All 2s
**Mask**: Index 8 (first tail element) masked
**Expected**: 65536 (2^16, skip element 8)
**Why it matters**:
- Tail element masked out, should skip it
- Tests that mask[8] and mask[16] handled differently
- Exposes: Bitset access across boundary at index 8 (start of second vector)
**Code path**: extract_8bits_from_bitset(mask, 8) at line 658

### Category 4: Power-of-2 Boundaries

These test behavior at exact vector boundaries (multiples of 8).

#### Test: Size256Int32Sum
**Size**: 256 elements (32 full vectors, no tail)
**Operation**: Sum
**Data**: All 1s
**Expected**: 256
**Why it matters**:
- Exactly on power-of-8 boundary
- Main loop should execute 32 times, i becomes 256
- Tail condition `if (i < num_elements)` is false (line 679)
- Exposes: Off-by-one in loop boundary calculation
**Code path**: Line 653: `num_full_vecs = 32`, loop runs 32×, i = 256, then line 679 is false

#### Test: Size257Int32Sum (FAILS - BUG #2)
**Size**: 257 elements (32 vectors + 1 tail)
**Operation**: Sum
**Data**: All 1s
**Expected**: 257
**Actual**: SIGSEGV
**Why it matters**:
- **EXPOSES BUG**: Crashes when loading tail past 256-element boundary
- Exactly 1 element in tail (tail_count=1)
- Line 694: _mm256_load_si256(data + 256) reads 8 elements, but only 1 allocated
- Exposes: Buffer overrun in SIMD load operation
**Code path**: Line 694: unsafe _mm256_load_si256 at large offset
**Root cause**: SIMD load always reads 8 elements regardless of tail_count

#### Test: Size1023Int32Sum, Size1024Int32Sum, Size1025Int32Sum
**Size**: 1023, 1024, 1025 elements
**Operation**: Sum
**Data**: All 1s
**Expected**: 1023, 1024, 1025
**Why it matters**:
- Tests behavior at 128-vector boundary (1024 = 128 × 8)
- Size 1025 similar to Size 257 but larger offset
- Exposes: Does crash only manifest at certain alignments?
**Code path**: Same as Size 256/257 but larger loop count

### Category 5: Mask Pattern Tests

These test specific mask patterns to verify mask bit extraction and blending.

#### Test: Size5SumTailAllMasked
**Size**: 5 elements
**Operation**: Sum
**Data**: [1, 2, 3, 4, 5]
**Mask**: All elements masked (all bits = 0)
**Expected**: 0 (identity for sum)
**Why it matters**:
- All elements present → all bits masked set to 0 in tail
- Tail blend should use identity value exclusively
- Exposes: tail_mask construction when all bits are zero
**Code path**: Lines 684-688: all tail_mask[j] = 0

#### Test: Size4MaxTailLastElementMasked
**Size**: 4 elements
**Operation**: Max
**Data**: [10, 50, 75, 100]
**Mask**: Last element (index 3) masked
**Expected**: 75
**Why it matters**:
- Last element in tail is masked out
- Max should find 75 (skip 100)
- tail_mask[3] = -1, tail_mask[4-7] = 0
- Exposes: Partial masking in tail element
**Code path**: Lines 684-690: loop runs j=0..3, checks each mask bit

#### Test: Size9AlternatingMaskSum
**Size**: 9 elements
**Operation**: Sum
**Data**: [1, 2, 3, 4, 5, 6, 7, 8, 9]
**Mask**: Even indices present (0, 2, 4, 6, 8)
**Expected**: 1 + 3 + 5 + 7 + 9 = 25
**Why it matters**:
- Tests alternating mask pattern
- Tail contains index 8 (present)
- Main loop processes indices 0-7 with alternating mask
- Exposes: Mask extraction from full vector (line 658)
**Code path**: extract_8bits_from_bitset(mask, 0) for vector 0, then tail at 679

### Category 6: Data Type Coverage

Float32 and BF16 versions of critical tests.

#### Test: Size5Float32Sum
**Size**: 5 elements
**Operation**: Sum
**Data**: [0, 0.5, 1.0, 1.5, 2.0] (float32)
**Expected**: 5.0
**Why it matters**:
- Tests float-specific accumulation
- Same tail logic but different SIMD operations (_mm256_add_ps vs _mm256_add_epi32)
- Exposes: Float identity (0.0f) vs int identity (0)
**Code path**: reduce_float32, lines 729-816

#### Test: Size9BF16Min
**Size**: 9 elements
**Operation**: Min
**Data**: [0, 1, 2, ..., 8] (bf16)
**Expected**: 0.0
**Why it matters**:
- Tests BF16 conversion to FP32 before reduction
- Same crash risk at Size 257 but with float precision
- Exposes: BF16-specific tail handling
**Code path**: reduce_bf16, lines 818-894

### Category 7: Boundary Value Tests

Tests with specific data patterns that expose state corruption.

#### Test: Size15AllTailMaskedMin
**Size**: 15 elements
**Operation**: Min
**Data**: [100, 99, ..., 86] (descending)
**Mask**: All tail elements masked (indices 8-14)
**Expected**: 93 (min of first 8)
**Why it matters**:
- Tail is entirely masked, should not affect min
- Tests identity blending for min (INT_MAX)
- If tail improperly processed, could override correct min
**Code path**: Main loop processes vector with all bits set, tail with all bits unset (line 779)

#### Test: Size9AllNegativeMin
**Size**: 9 elements
**Operation**: Min
**Data**: [-1, -2, ..., -9]
**Expected**: -9
**Why it matters**:
- Min of all negative numbers
- Tests signed integer edge case
- Exposes: Sign extension in tail mask construction (tail_mask[j] = -1 = 0xFFFFFFFF)
**Code path**: Lines 684-688: tail_mask[j] must be -1 (not 1)

## Test Execution Order Analysis

### Why Size 17 Min Fails but Size 9 Min Passes

| Aspect | Size 9 | Size 17 | Size 257 |
|--------|--------|---------|----------|
| Full vectors | 1 | 2 | 32 |
| Tail elements | 1 | 1 | 1 |
| Main loop iterations | 1 | 2 | 32 |
| Accumulator state | 1 min | min(v0, v1) | min(v0..v31) |
| Tail blend result | Should be 1 | Should be 84 | SIGSEGV |
| Test outcome | PASS ✓ | FAIL ✗ | FAIL ✗ |

**Hypothesis**: Bug manifests when:
1. Two+ vectors before tail (accumulator has state from v1)
2. Tail single element needs to override
3. Horizontal reduction reads from unoverridden lane

## Code Paths Exercised

### Main Loop (Line 653-676)
- Size 8+: Executed at least once
- Size 256: Executed 32 times
- Tests that verify: Vector extraction, mask expansion, accumulation

### Tail Processing (Line 679-708)
- Size < 8: Only path executed
- Size 8: Never executed (i=8, num_elements=8, condition false)
- Size 9+: Always executed exactly once per tail
- Tests that verify: Tail count calculation, mask construction, blend, accumulation

### Horizontal Reduction (Line 711-726)
- All tests execute this (final step)
- Different paths for sum/product/min/max
- Tests that verify: Final accumulator state extraction

## Critical Code Sections

### Tail Mask Construction (Lines 684-688)
```cpp
for (std::size_t j = 0; j < tail_count; ++j) {
  if (mask[i + j]) {
    tail_mask[j] = -1; // 0xFFFFFFFF
  }
}
```
**Risk**: Off-by-one in loop, incorrect bitset access at mask[i+j]
**Tests**: Size2-7 basic, Size9AlternatingMaskSum alternating, Size4MinTailFirstElementMasked selective

### SIMD Tail Load (Line 693-694)
```cpp
__m256i tail_data = _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i));
```
**Risk**: Reads 8 elements even if tail_count < 8, possible buffer overrun
**Tests**: Size257Int32Sum (CRASHES), Size1025Int32Sum
**Failure**: SIGSEGV when offset + 8 > allocation

### Tail Blend (Line 695-696)
```cpp
__m256i blended_tail = _mm256_blendv_epi8(identity_vec, tail_data, simd_tail_mask);
```
**Risk**: Blend doesn't overwrite all necessary lanes, stale accumulator values remain
**Tests**: Size17Int32Min (RETURNS WRONG VALUE), Size9ProductWithTailMasked
**Failure**: Tail value doesn't replace existing accumulator state

### Horizontal Reduction Min (Line 723)
```cpp
return horizontal_min_epi32(accumulator);
```
**Risk**: If blended_tail didn't properly update accumulator lane, wrong minimum is returned
**Tests**: Size17Int32Min (FAILS - returns 84 not 83)

## Test Dependencies

- **Prerequisite tests** (must pass before Size 9+):
  - Size2, Size3, Size4, Size5, Size6, Size7 (pure tail tests)

- **Foundational tests** (Size 9 must pass):
  - Size9Int32Sum
  - Size9Int32Product
  - Size9Int32Min
  - Size9Int32Max

- **Bug-exposing tests** (fail if foundational tests pass):
  - Size17Int32Min (logic bug if others work)
  - Size257Int32Sum (segfault if boundary safe)

## Recommendations for Test Maintenance

1. **Keep Size 2-7 tests**: Critical for pure tail verification
2. **Keep Size 9 tests**: Foundation for all tail tests
3. **Keep Size 17 tests**: First place logic bugs appear
4. **Keep Size 257 tests**: Safety boundary test
5. **Add Size 1025 tests**: Double-check large offsets
6. **Add Size 65537 tests**: Triple-check very large offsets

## Using Tests to Debug Fixes

When fixing Bug #1 (Size 17 Min):
1. Verify Size 9 Min still passes
2. Verify Size 17 Min now passes
3. Verify Size 31 Min passes
4. Verify Size 257 Min (currently crashes) now works

When fixing Bug #2 (Size 257 crash):
1. Verify Size 257 Sum no longer crashes
2. Verify all Size 257 operations (sum, product, min, max) work
3. Verify Size 1025 Sum doesn't crash
4. Verify Size 65537 Sum doesn't crash

## Summary

The 53 tests systematically cover:
- **6 size categories**: Pure tail (2-7), one vector (9-15), two vectors (17-31), boundaries (255-257, 1023-1025), and extremes
- **4 operations**: sum, product, min, max
- **3 data types**: int32, float32, bf16
- **6 mask patterns**: all-present, all-missing, selective, alternating, boundary, mixed
- **Result**: 2 critical bugs exposed, ready for fixing
