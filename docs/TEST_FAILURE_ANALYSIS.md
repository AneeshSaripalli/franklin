# Test Failure Analysis - Math Utils Adversarial Tests

## Overview

This document provides detailed analysis of why each test fails, what the failures reveal about the code, and what the correct behavior should be.

---

## Test Failure #1: IsPow2Test.ZeroIsNotPowerOfTwo

### Test Code
```cpp
TEST(IsPow2Test, ZeroIsNotPowerOfTwo) {
  EXPECT_FALSE(is_pow2(static_cast<std::uint8_t>(0)));
  EXPECT_FALSE(is_pow2(static_cast<std::uint16_t>(0)));
  EXPECT_FALSE(is_pow2(static_cast<std::uint32_t>(0)));
  EXPECT_FALSE(is_pow2(static_cast<std::uint64_t>(0)));
}
```

### Actual Output
```
[ RUN      ] IsPow2Test.ZeroIsNotPowerOfTwo
core/math_utils_test.cpp:21: Failure
Value of: is_pow2(static_cast<std::uint8_t>(0))
  Actual: true
Expected: false
0 is not a power of 2, but is_pow2(0) returns true

core/math_utils_test.cpp:23: Failure
Value of: is_pow2(static_cast<std::uint16_t>(0))
  Actual: true
Expected: false
...
[  FAILED  ] IsPow2Test.ZeroIsNotPowerOfTwo (0 ms)
```

### Why It Fails

**Mathematical Definition:** Powers of 2 are numbers of the form 2^n where n is a non-negative integer.
- Valid powers of 2: 1, 2, 4, 8, 16, 32, 64, 128, 256, ...
- Invalid: 0, 3, 5, 6, 7, 9, 10, ...
- Zero is NOT a power of 2

**Algorithm Analysis:**
```cpp
template <std::unsigned_integral T>
static constexpr bool is_pow2(T x) noexcept {
  return (x & (x - 1)) == 0;
}
```

For x = 0:
- `0 - 1 = 0xFFFFFFFF...` (all bits set, due to two's complement underflow)
- `0 & 0xFFFFFFFF = 0`
- `0 == 0` is true
- Result: is_pow2(0) returns true (WRONG!)

For x = 1:
- `1 - 1 = 0`
- `1 & 0 = 0`
- `0 == 0` is true
- Result: is_pow2(1) returns true (CORRECT - 1 = 2^0)

For x = 2:
- `2 - 1 = 1` (binary: 10 & 01)
- `2 & 1 = 0`
- `0 == 0` is true
- Result: is_pow2(2) returns true (CORRECT - 2 = 2^1)

For x = 3:
- `3 - 1 = 2` (binary: 11 & 10)
- `3 & 2 = 2`
- `2 == 0` is false
- Result: is_pow2(3) returns false (CORRECT)

**The Bug:** The bit-trick works for all positive integers but fails for zero because zero is the only non-negative integer where subtracting 1 wraps around to the maximum value in two's complement.

### Correct Behavior

```cpp
// Current (WRONG):
return (x & (x - 1)) == 0;

// Correct:
return (x != 0) && (x & (x - 1)) == 0;
```

With the fix:
- `is_pow2(0)` → `(0 != 0) && ... ` → `false && ...` → **false** ✓
- `is_pow2(1)` → `(1 != 0) && (0)` → `true && true` → **true** ✓
- `is_pow2(2)` → `(2 != 0) && (0)` → `true && true` → **true** ✓
- `is_pow2(3)` → `(3 != 0) && (2)` → `true && false` → **false** ✓

### Impact on Production Code

Any code that validates a size using is_pow2 will incorrectly accept 0:

```cpp
// Example 1: Buffer allocation
void allocate_buffer(uint32_t size) {
  if (!is_pow2(size)) {
    throw std::invalid_argument("Size must be power of 2");
  }
  allocate(size);  // BUG: Can now allocate 0 bytes!
}

// Example 2: Power-of-2 lookup table
void init_cache(uint32_t size) {
  if (is_pow2(size)) {
    cache.reserve(size);  // BUG: 0-sized reservation
  }
}

// Example 3: Bit-width calculation
uint32_t get_bits_needed(uint32_t max_val) {
  return 32 - __builtin_clz(max_val); // BUG: undefined for 0
}
```

---

## Test Failure #2: IsPow2Test.ConstexprEvaluation

### Test Code
```cpp
TEST(IsPow2Test, ConstexprEvaluation) {
  constexpr bool test_zero = is_pow2(static_cast<std::uint8_t>(0));
  constexpr bool test_one = is_pow2(static_cast<std::uint8_t>(1));
  constexpr bool test_two = is_pow2(static_cast<std::uint8_t>(2));
  constexpr bool test_three = is_pow2(static_cast<std::uint8_t>(3));

  EXPECT_FALSE(test_zero);
  EXPECT_TRUE(test_one);
  EXPECT_TRUE(test_two);
  EXPECT_FALSE(test_three);
}
```

### Actual Output
```
[ RUN      ] IsPow2Test.ConstexprEvaluation
core/math_utils_test.cpp:129: Failure
Value of: test_zero
  Actual: true
Expected: false

[  FAILED  ] IsPow2Test.ConstexprEvaluation (0 ms)
```

### Why It Fails

This is the same bug as Test Failure #1, but evaluated at compile-time. The `constexpr` keyword forces compilation-time evaluation:

```cpp
// At compile time:
constexpr bool test_zero = is_pow2(static_cast<std::uint8_t>(0));
// Evaluates to:
constexpr bool test_zero = (0 & (0 - 1)) == 0;
// Which is:
constexpr bool test_zero = (0 & 0xFF) == 0;
// Which is:
constexpr bool test_zero = 0 == 0;
// Which is:
constexpr bool test_zero = true;  // WRONG!
```

### Impact on Compile-Time Optimization

```cpp
// Example: Compile-time validation
template <uint8_t Size>
struct StaticArray {
  static_assert(is_pow2(Size), "Size must be power of 2");
  uint8_t data[Size];
};

StaticArray<0> arr;  // BUG: Compiles when it shouldn't!
```

---

## Test Failure #3: IsPow2LogicBug.ZeroEdgeCase

### Test Code
```cpp
TEST(IsPow2LogicBug, ZeroEdgeCase) {
  EXPECT_FALSE(is_pow2(std::uint64_t{0}))
      << "BUG: is_pow2(0) returns true, but 0 is not a power of 2. "
      << "The bit trick (x & (x-1)) == 0 is only valid for x > 0.";
}
```

### Actual Output
```
[ RUN      ] IsPow2LogicBug.ZeroEdgeCase
core/math_utils_test.cpp:213: Failure
Value of: is_pow2(std::uint64_t{0})
  Actual: true
Expected: false
BUG: is_pow2(0) returns true, but 0 is not a power of 2. The bit trick (x & (x-1)) == 0 is only valid for x > 0.

[  FAILED  ] IsPow2LogicBug.ZeroEdgeCase (0 ms)
```

### Why It Fails

Same root cause as Failures #1 and #2, just tested with uint64_t to ensure the bug is consistent across all supported types.

---

## Test Skip #1: RoundPow2Test.CompilationErrorInRoundPow2ForUint32

### Test Code
```cpp
TEST(RoundPow2Test, CompilationErrorInRoundPow2ForUint32) {
  GTEST_SKIP() << "round_pow2 has compilation errors: size(x) and sizef(x) undefined";
}
```

### Why It's Skipped (Not Tested)

This test cannot actually test round_pow2 because the function fails to compile. The undefined functions prevent instantiation.

### The Real Issue

```cpp
template <std::unsigned_integral T>
static constexpr T round_pow2(T x) noexcept {
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  if constexpr (sizeof(x) > 1)
    x |= x >> 8;
  if constexpr (size(x) > 2)    // LINE 21: ERROR - 'size' is undefined
    x |= x >> 16;
  if constexpr (sizef(x) > 4)   // LINE 23: ERROR - 'sizef' is undefined
    x |= x >> 32;
  return ++x;
}
```

When the compiler tries to instantiate `round_pow2<uint32_t>`:

1. Template instantiation begins
2. Line 19: `if constexpr (sizeof(uint32_t) > 1)` → true, executes line 20
3. Line 21: `if constexpr (size(uint32_t) > 2)` → **ERROR: 'size' is not declared**
4. Compilation fails

### Algorithm Explanation (What It Was Supposed To Do)

The function attempts to fill lower bits using a doubling-shift pattern:

```
For value: 0b0001_0101 (21)

Step 1: x |= x >> 1   // 0b0001_0101 | 0b0000_1010 = 0b0001_1111
Step 2: x |= x >> 2   // 0b0001_1111 | 0b0000_0111 = 0b0001_1111
Step 3: x |= x >> 4   // 0b0001_1111 | 0b0000_0001 = 0b0001_1111
Result: All lower bits set = 0b0001_1111 = 31
Then: ++x = 32 = 2^5

So round_pow2(21) should return 32 (next power of 2)
```

But for **uint32_t**, this algorithm needs additional steps:

```
Step 1: x |= x >> 1   // Fills 2 bits
Step 2: x |= x >> 2   // Fills 4 bits
Step 3: x |= x >> 4   // Fills 8 bits
Step 4: x |= x >> 8   // Fills 16 bits
Step 5: x |= x >> 16  // Fills 32 bits  <-- MISSING! (Typo prevents this)
```

Without step 5, high bits won't be filled, causing incorrect results for values with bits set above bit 16.

### Expected Behavior (After Fix)

```cpp
// Change line 21 from:
if constexpr (size(x) > 2)
// To:
if constexpr (sizeof(x) > 2)

// Change line 23 from:
if constexpr (sizef(x) > 4)
// To:
if constexpr (sizeof(x) > 4)
```

After the fix:
- `sizeof(uint32_t) = 4`, so `sizeof(x) > 2` is true → line 22 executes ✓
- The x |= x >> 16 shift happens
- Algorithm works correctly for 32-bit values

---

## Test Skip #2: RoundPow2Test.CompilationErrorInRoundPow2ForUint64

### Why It's Skipped

Same as Skip #1, but for 64-bit types. The undefined functions prevent compilation.

For **uint64_t**, both missing shifts are needed:

```
sizeof(uint64_t) = 8

Step 1: x |= x >> 1   // Fills 2 bits
Step 2: x |= x >> 2   // Fills 4 bits
Step 3: x |= x >> 4   // Fills 8 bits
Step 4: x |= x >> 8   // Fills 16 bits
Step 5: x |= x >> 16  // Fills 32 bits  <-- BLOCKED by typo
Step 6: x |= x >> 32  // Fills 64 bits  <-- BLOCKED by typo
```

Without both steps 5 and 6, the algorithm fails completely for 64-bit values.

---

## Test Skip #3 & #4: NextPow2Test Failures

### Why They're Skipped

Identical reason as the round_pow2 tests - the same typos appear in next_pow2:

```cpp
template <std::unsigned_integral T>
static constexpr T next_pow2(T x) noexcept {
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  if constexpr (sizeof(x) > 1)
    x |= x >> 8;
  if constexpr (size(x) > 2)    // LINE 34: ERROR - 'size' is undefined
    x |= x >> 16;
  if constexpr (sizef(x) > 4)   // LINE 36: ERROR - 'sizef' is undefined
    x |= x >> 32;
  return ++x;
}
```

### Difference Between round_pow2 and next_pow2

While they have the same underlying typos, their semantic difference is:

**round_pow2(x):** Round UP to next power of 2 (or same if already power of 2)
- round_pow2(8) should return 8 (already power of 2)
- round_pow2(9) should return 16

**next_pow2(x):** Get the NEXT power of 2 AFTER x (always strictly greater)
- next_pow2(8) should return 16 (next power after 8)
- next_pow2(9) should return 16 (or could be 16 depending on implementation)

The key difference: round_pow2 uses `--x` first, next_pow2 does not. This causes their behavior to differ for powers of 2.

But both have identical compilation errors that prevent them from working at all.

---

## Summary of Bugs Exposed

| Bug | Type | Severity | Functions Affected | Test Status |
|-----|------|----------|-------------------|-------------|
| Zero case in is_pow2 | Logic | High | is_pow2() | FAILED (3 tests) |
| Typo: size() undefined | Compilation | Critical | round_pow2(), next_pow2() | SKIPPED (4 tests) |
| Typo: sizef() undefined | Compilation | Critical | round_pow2(), next_pow2() | SKIPPED (4 tests) |

---

## Quick Reference: What the Tests Proved

### Proved Working
- is_pow2() correctly identifies all actual powers of 2 (1,2,4,8,...)
- is_pow2() correctly rejects all non-powers of 2 (3,5,6,7,...)
- is_pow2() correctly rejects max values and max-1

### Proved Broken
- is_pow2(0) returns true (should return false)
- is_pow2() at compile time fails on zero (should work)
- round_pow2() fails to compile for uint32_t and uint64_t
- next_pow2() fails to compile for uint32_t and uint64_t

### Proved Untestable (Due to Compilation Errors)
- Actual correctness of round_pow2() algorithm
- Actual correctness of next_pow2() algorithm
- Their behavior on edge cases
- Their performance characteristics

---

## What Makes These Tests "Adversarial"

1. **Edge Case Coverage:** Tests specifically target boundary conditions (0, 1, max values) that often reveal logic errors

2. **Type Genericity:** Tests verify behavior across all supported types (uint8_t, uint16_t, uint32_t, uint64_t) to catch type-specific bugs

3. **Exhaustive Iteration:** Small types are tested exhaustively (all 8 powers for uint8_t) to catch off-by-one errors

4. **Compile-Time Verification:** Tests include constexpr evaluation to catch bugs that only show up at compile-time

5. **Root Cause Documentation:** Each test includes comments explaining the mathematical reasoning and expected behavior

6. **Failure Clarity:** Tests include detailed assertion messages that explain WHY the current behavior is wrong

---
