# Non-Power-of-2 Size Test Coverage Analysis

## Executive Summary

Current test coverage for reduction operations is **severely deficient** in non-power-of-2 scenarios. The tests exercise only **6 different array sizes** (0, 1, 4, 5, 8, 16), with heavy bias toward power-of-2 and tiny arrays. This leaves critical tail-handling edge cases completely untested.

## Current Coverage

### Sizes Currently Tested
- Size 0: Empty (1 test)
- Size 1: Single element (2 tests)
- Size 4: Partial vector (1 test)
- Size 5: One vector + tail (1 test)
- Size 8: Exactly one vector (1 test)
- Size 16: Two full vectors (21 tests)

### Coverage Matrix by Operation and Type
```
Operation  Int32    Float32  BF16     Notes
-----      -----    -------  ----     -----
Sum        Tests    Tests    Tests    Only sizes: 0,1,5,8,16
Product    Tests    Tests    Tests    Only sizes: 0,1,4,5,8
Min/Max    Tests    Tests    Tests    Only sizes: 0,1,8,16
Any/All    Partial  --       --       Only size 0,1,16
```

## Critical Gap Analysis

### 1. Sizes NOT Tested (Tail-Specific Scenarios)

#### Pure Tail Cases (No Full Vectors)
- Size 2, 3, 4, 5, 6, 7: Only tail handling, no full 8-element vectors
- **Risk**: Uninitialized accumulator state, incorrect identity blending
- **Evidence**: Main loop never executes (num_full_vecs = 0)

#### Exact Boundary Cases
- Size 8: Exactly one vector (barely tested at 1 test)
- Size 15: One vector + 7-element tail
- Size 16: Exactly two vectors (adequate)
- Size 17: Two vectors + 1-element tail

#### Power-of-2 Boundaries
- Size 256: 32 vectors exactly
- Size 257: 32 vectors + 1-element tail
- Size 1024: 128 vectors exactly
- Size 1025: 128 vectors + 1-element tail
- Size 65536: 8192 vectors exactly
- Size 65537: 8192 vectors + 1-element tail

#### Prime Numbers (Worst-Case for Tail)
- Size 7, 13, 17, 31, 127, 251
- These guarantee non-aligned tail, no convenient divisibility

### 2. Mask Pattern Gaps

Current tests only use:
- All elements present (most tests)
- All elements missing (1 test per operation)
- Sparse missing at indices 0, 5, 15 (1 test per operation)

**Missing patterns**:
- Tail elements specifically masked (e.g., mask indices 8-15 in size 16)
- All tail elements masked in small arrays
- Alternating mask patterns (reveals stride issues)
- Mask concentrated in different tail positions

### 3. Tail-Specific Test Gaps

Current tests rely on implicit tail correctness through large array tests (size 1017, 1013).

**Missing explicit tail tests**:
- Size 1-7: Pure tail, no full vectors
- Size 9-15: One vector + tail (critical!)
- Size 17-31: Two vectors + tail
- Size 127-128: Around 16-vector chunk boundary
- Size 255-257: Around 32-vector chunk boundary

## Logic Bombs: Scenarios Where Implementation Could Fail

### Bug 1: Uninitialized Accumulator in Pure Tail Cases
**Scenario**: Size <= 7 (no full vectors)
- Main loop never executes: `num_full_vecs = 0`
- Accumulator initialized once but never used in main loop
- Tail processing relies entirely on mask blending

**Failure mode**: If tail mask construction is incorrect, all results wrong.

**Test exposure**: Size 3 with all elements present
- Expected: 1 + 2 + 3 = 6
- Failure: Could return identity (0) if blend is inverted

### Bug 2: Tail Mask Boundary Conditions
**Scenario**: Tail count = 1 (e.g., size 9, 17, 25)
- Only first element of tail matters
- tail_mask[0] set, tail_mask[1..7] = 0
- Uninitialized tail data[i+1..7] could leak through

**Failure mode**: Wrong blend if identity_vec not used properly
- Products should stay 1, not multiply by garbage
- Min/Max should return identity, not stale register values

**Test exposure**: Size 9 product (identity=1)
- Data: [2, 3, 4, 5, 6, 7, 8, 9, 10]
- Expected: 2*3*4*5*6*7*8*9*10 = 3,628,800
- Failure: Could be 3,628,800 * garbage_value

### Bug 3: Multiple Tail Processing
**Scenario**: Refactor moves tail to separate function
- Duplicate tail handling in main loop and separate path
- If only one path gets fixed, silent corruption

**Failure mode**: Size 16 with special mask patterns
- Tests never explored mask variations in full-vector tail path
- Main loop tail (vectors 0-7) different from explicit tail (vectors 8-15)

**Test exposure**: Size 16 with mask = all 1s except positions 8-10
- Should work correctly but never explicitly tested

### Bug 4: Horizontal Reduction After Partial Accumulation
**Scenario**: Product reduction, all tail elements masked
- Tail blending produces identity values (1.0f) in unprocessed lanes
- Horizontal reduction multiplies all 8 lanes together
- If accumulator lane has stale non-1 value, product wrong

**Test exposure**: Size 9 product, mask only first 8 elements
- Data: [1, 1, 1, 1, 1, 1, 1, 1, 2]
- Mask: elements 0-7 present, element 8 missing
- Expected: 1*1*1*1*1*1*1*1*1 = 1
- Failure: Could be 1 * stale_value or 1 * 2 if blend fails

### Bug 5: Min/Max Identity Not Applied Correctly in Tail
**Scenario**: All tail elements masked, full vectors have no extreme values
- Min reduction expects identity (INT_MAX) for masked lanes
- If tail_mask build wrong, stale data pollutes min calculation

**Test exposure**: Size 9 min, data all 50s, mask only full vector
- Data: [50, 50, 50, 50, 50, 50, 50, 50, X]
- Mask: 8 present, 1 absent
- Expected: 50
- Failure: Could be min(50, X) if X is garbage or if identity not blended

### Bug 6: Off-by-One in Tail Element Counting
**Scenario**: tail_count = num_elements - i
- If i calculation wrong after main loop, tail_count wrong
- Could over-read or under-read mask

**Test exposure**: Size 1025 (128 vectors + 1 tail)
- After main loop: i = 1024, num_elements = 1025
- tail_count = 1025 - 1024 = 1 (correct)
- Mask processing should only check mask[1024]
- Failure: If loop counts wrong, accesses beyond bitset

### Bug 7: BF16 Conversion Accumulation Loss
**Scenario**: BF16 -> FP32 for reduction, then back
- Sum of many small BF16 values loses precision
- Different code path than int32/float32
- Tail handling same code but working with converted values

**Test exposure**: Size 17 BF16 sum of small values
- Data: [0.1f, 0.1f, ..., 0.1f] (17 times) = 1.7f
- Full vectors: [0.1f]*8 summed twice = 0.8f + 0.8f = 1.6f
- Tail: [0.1f] = 0.1f
- Total: 1.7f
- Failure: Precision loss if tail conversion separate from full vector

## Critical Test Cases by Priority

### Priority 1: Pure Tail (No Full Vectors)
These expose uninitialized state bugs immediately:
- Size 1: Single element
- Size 2: Two elements
- Size 3: Three elements
- Size 4: Four elements
- Size 5: Five elements (already tested, but limited)
- Size 6: Six elements
- Size 7: Seven elements

**For each**: Test all 4 operations (sum/product/min/max) × 3 types (int32/float32/bf16) × 3 mask patterns (all present/all missing/mixed)
= 108 tests minimum

### Priority 2: One Vector + Tail
These test main loop → tail transition:
- Size 9: One vector + 1 tail
- Size 10: One vector + 2 tail
- Size 15: One vector + 7 tail (critical, near power-of-2)

**For each**: All 4 operations × 3 types × mask variations
= 36 tests minimum

### Priority 3: Power-of-2 Boundaries
These test accumulator state across chunk boundaries:
- Size 256, 257
- Size 1024, 1025
- Size 65536, 65537

### Priority 4: Tail Mask Patterns
These test specific mask scenarios:
- Tail all present/absent
- Tail alternating present/absent
- Only first tail element present
- Only last tail element present

## Recommendations

1. **Before Refactoring**: Generate failing tests for all sizes 1-31, 255-257, 1023-1025
2. **Use Parameterized Tests**: Template over (size, operation, type, mask_pattern)
3. **Test Transitions**: Explicitly test i, num_full_vecs, tail_count calculations
4. **Mask Variations**: For each size, test all-present, all-absent, and targeted masks
5. **Sanity Checks**: Verify that reduction is commutative (where applicable)

## Test Generation Strategy

For each test size S and operation OP:
1. Generate reference output via scalar reduction
2. Generate SIMD output via column_vector::OP()
3. Compare with tolerance (stricter for int, wider for BF16)
4. Vary mask patterns to expose blend/identity bugs

Example:
```cpp
// Size 9, product, int32, all present
auto col = column_vector<Int32DefaultPolicy>(9);
for (int i = 0; i < 9; ++i) col.data()[i] = i + 2;
EXPECT_EQ(col.product(), 2*3*4*5*6*7*8*9*10);  // Should be 3,628,800
```
