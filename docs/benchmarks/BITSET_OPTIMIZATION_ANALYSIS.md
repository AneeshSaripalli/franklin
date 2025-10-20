# Dynamic Bitset AVX2 Optimization Analysis

## Summary

Comprehensive benchmarking of scalar vs AVX2-optimized implementations of `any()`, `none()`, and `all()` operations with various bit densities (0%, 1%, 5%, 10%, 20%, 50%, 80%, 100%).

## Key Findings

### 1. any() Operation

**Worst Case (0% density - full scan required):**
- **Small (1024 bits)**: Scalar 28 GB/s vs AVX2 74 GB/s → **2.6x speedup**
- **Medium (65536 bits)**: Scalar 39 GB/s vs AVX2 238 GB/s → **6.1x speedup**
- **Large (1 MB)**: Scalar 39 GB/s vs AVX2 155 GB/s → **4.0x speedup**

**Best Case (1-80% density - early exit on average):**
- **All sizes**: Scalar ~0.29 ns vs AVX2 ~0.78 ns → **Scalar 2.7x faster**
- Reason: Early exit happens before SIMD overhead pays off

### 2. all() Operation

**Worst Case (100% density - full scan required):**
- **Small (1024 bits)**: Scalar 27 GB/s vs AVX2 153 GB/s → **5.7x speedup**
- **Medium (65536 bits)**: Scalar 38 GB/s vs AVX2 200 GB/s → **5.3x speedup**
- **Large (1 MB)**: Scalar 39 GB/s vs AVX2 156 GB/s → **4.0x speedup**

**Best Case (0-80% density - early exit on first zero):**
- **All sizes**: Scalar ~0.58 ns vs AVX2 ~0.78 ns → **Scalar 1.3x faster**

### 3. none() Operation

Identical performance profile to `any()` (implemented as `!any()`).

## Performance Characteristics

### AVX2 Implementation
- **Strengths**:
  - Processes 64 bytes (1 cache line) per iteration
  - 4-6x faster for full scans on medium-large datasets (>32KB)
  - Memory bandwidth saturated at ~155-240 GB/s
  - Cache-line aligned allocations reduce false sharing

- **Weaknesses**:
  - ~0.5ns overhead for SIMD setup
  - Slower for small datasets (<1024 bits)
  - Slower when early exits are common (1-80% density)

### Scalar Implementation
- **Strengths**:
  - Zero overhead for small datasets
  - Excellent early-exit performance (0.29-0.58 ns)
  - Better for sparse/dense data with expected early exits

- **Weaknesses**:
  - Memory bandwidth limited to ~39 GB/s
  - 4-6x slower for full scans on large datasets

## Crossover Points

| Size      | any()/none() | all()      | Winner |
|-----------|--------------|------------|--------|
| <1KB      | All densities| All densities | Scalar |
| 1-4KB     | 0% density   | 100% density  | AVX2   |
| 1-4KB     | 1-80% density| 0-80% density | Scalar |
| >32KB     | 0% density   | 100% density  | AVX2   |
| >32KB     | 1-80% density| 0-80% density | Tie/Scalar |

## Recommendations

### Option 1: Hybrid Implementation (Recommended)
Use runtime dispatch based on bitset size:
```cpp
bool any() const noexcept {
  if (blocks_.size() < 8) {  // <512 bytes
    return any_scalar();
  } else {
    return any_avx2();
  }
}
```

**Pros**: Best of both worlds
**Cons**: Additional branch, code complexity

### Option 2: Pure AVX2 (For Large Data Workloads)
Use AVX2 for all sizes if your workload primarily involves:
- Large bitsets (>32KB)
- Full scans (0% or 100% density)

**Pros**: Simple, maximum throughput for large data
**Cons**: 2-3x slower for small datasets and early exits

### Option 3: Pure Scalar (For Small/Sparse Data)
Keep current scalar implementation if your workload involves:
- Small bitsets (<4KB)
- Frequent early exits (1-80% density)

**Pros**: Simple, excellent small-data performance
**Cons**: Missing 4-6x speedup for large full scans

## Memory Bandwidth Analysis

| Implementation | Observed BW | Theoretical Max | Efficiency |
|----------------|-------------|-----------------|------------|
| Scalar         | 39 GB/s     | ~50 GB/s (1ch)  | 78%        |
| AVX2           | 155-240 GB/s| ~300 GB/s (DDR) | 52-80%     |

AVX2 achieves 4-6x scalar bandwidth, approaching memory limits.

## Cache Effects

- **L1 Cache (32 KB)**: Both implementations perform well
- **L2 Cache (1 MB)**: AVX2 advantage grows to 6x
- **L3/DRAM**: AVX2 maintains 4x advantage, memory-bound

## Conclusion

**For general-purpose use with unknown workload characteristics**, implement **Option 1 (Hybrid)** with size-based dispatch. The small overhead of the branch is negligible compared to the 4-6x speedup on large datasets.

**Current recommendation**:
- Integrate AVX2 implementation with 8-block threshold (512 bytes)
- Add compile-time flags to disable AVX2 for embedded/low-power targets
- Consider PGO (Profile-Guided Optimization) for production deployments
