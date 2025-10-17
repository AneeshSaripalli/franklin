# Dynamic Bitset Bugs - Quick Reference

## 3 Failing Tests Expose 3 Critical Bugs

### Test Failure #1: ResizeWithinBlockValueTrueDoesntSetNewBits

**What fails:**
```cpp
Bitset bs(10, false);
bs.resize(20, true);
// Expected: bits 10-19 set to true
// Actual: bits 10-19 are false
```

**Root cause:** resize() doesn't initialize new bits when growing within same block count

**Fix:** Set bits in range [old_num_bits, new_num_bits) when value=true

---

### Test Failure #2: CountWithPartialBlockUnusedBits

**What fails:**
```cpp
Bitset bs(70, true);  // All 70 bits set
bs.resize(65);        // Shrink to 65
EXPECT_EQ(bs.count(), 65);  // FAILS: returns 70
```

**Root cause:** resize() doesn't clear stale bits when shrinking

**Fix:** Always call zero_unused_bits() after shrinking

---

### Test Failure #3: BitwiseOpsPreserveLHSSize

**What fails:**
```cpp
Bitset bs1(100, false);
Bitset bs2(50, false);
bs1.set(70);
bs1 &= bs2;
EXPECT_FALSE(bs1.test(70));  // FAILS: bit 70 still set
```

**Root cause:** operator&= only processes min(blocks), leaving rest unchanged

**Fix:** Clear blocks beyond RHS size (OR report error on size mismatch)

---

## How to Run Tests

```bash
# Run all tests
bazel test //core:all --test_output=errors

# Run specific test
bazel test //core:dynamic_bitset_test --test_output=all

# Run with verbose output
bazel test //core:dynamic_bitset_test --test_output=streamed
```

---

## Test Results Summary

- Total: 44 tests
- Passed: 41 tests (93.2%)
- Failed: 3 tests (6.8%)
- All failures are data corruption bugs that would silently produce wrong results

---

## Files to Review

1. `/home/remoteaccess/code/franklin/container/dynamic_bitset.hpp` - Implementation with bugs
2. `/home/remoteaccess/code/franklin/container/dynamic_bitset_test.cpp` - Test suite (29 tests)
3. `/home/remoteaccess/code/franklin/container/dynamic_bitset_stress_test.cpp` - Stress tests (15 tests)
4. `/home/remoteaccess/code/franklin/container/dyNAMIC_BITSET_BUG_REPORT.md` - Detailed bug analysis
5. `/home/remoteaccess/code/franklin/TESTING_SUMMARY.md` - Complete testing summary
