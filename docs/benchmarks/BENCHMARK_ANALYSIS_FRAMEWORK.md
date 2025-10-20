# Franklin Benchmark Analysis Framework

## Overview

This document describes the complete benchmark framework for analyzing memory performance in Franklin, with emphasis on the new cache throughput baseline measurements and how they relate to existing benchmarks.

## Architecture

```
Application Benchmarks        Baseline Measurements
├─ column_benchmark           └─ cache_throughput_benchmark
├─ column_reduction_benchmark    (L1/L2/L3/DRAM throughput)
└─ kernel_fusion_benchmark
    ↓
    Measure throughput (GB/s)
    ↓
    Compare against cache_throughput_benchmark
    ↓
    Calculate cache utilization
    ↓
    Profile with perf/VTune (if <50% utilization)
```

## Step-by-Step Analysis Workflow

### Step 1: Establish Cache Baseline (One-Time)

Run the cache throughput benchmark on your target machine to establish the hardware ceiling:

```bash
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_min_time=3s
```

Record the results in a file for reference:
```bash
bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_min_time=3s \
  --benchmark_out_format=csv \
  --benchmark_out=baseline_cache.csv
```

This gives you the hardware limits:
- L1 throughput ceiling
- L2 throughput ceiling
- L3 throughput ceiling
- DRAM throughput ceiling

For each operation type (Load, Store, Mixed).

### Step 2: Measure Application Performance

Run your application benchmark:

```bash
bazel run //benchmarks:column_benchmark -- \
  --benchmark_filter="BM_Int32_Add" \
  --benchmark_min_time=3s \
  --benchmark_out_format=csv \
  --benchmark_out=app_results.csv
```

Extract the throughput (GB/s) and working set size for the operation.

### Step 3: Analyze Utilization

Calculate how much of available bandwidth your operation achieves:

```
utilization = (app_throughput_GB_s / cache_throughput_GB_s) * 100%
```

**Example Analysis:**

```
Operation: Int32 column addition
Working set: 12 MB (3 columns)
Cache fit: L3 (32 MB)
Application throughput: 45 GB/s
Cache baseline (mixed, L3): 120 GB/s
Utilization: 45 / 120 = 37.5%

Interpretation:
- Not memory-bound (utilization < 80%)
- Could achieve better throughput with optimization
- Likely limited by: instruction latency or branch misprediction
```

### Step 4: Root Cause Analysis

If utilization is below expected, determine the limiting factor:

#### 4a. Is it a memory issue?

Run with profiling:
```bash
perf record -e cycles,cache-references,cache-misses,instructions \
  bazel run //benchmarks:column_benchmark -- BM_Int32_Add

perf report
```

High cache-misses/instructions ratio indicates:
- Working set doesn't fit expected cache
- Access pattern causes misses (non-sequential, random, etc.)
- False sharing (multi-threaded code)

#### 4b. Is it a compute issue?

Look at instructions/cycle ratio:
- If <1 IPC (instructions per cycle): pipeline stalls, data dependencies
- If 1-2 IPC: reasonable for simple operations
- If >2 IPC: good instruction-level parallelism

#### 4c. Specific cache level

Compare against cache level throughput:
- If app ~= L1 throughput: fits in L1, good
- If app << L1 throughput but fits in L1: instruction issue, not memory
- If app ~= L2 throughput: fits in L2, hitting L1 misses
- If app ~= L3 throughput: fits in L3, hitting L2 misses
- If app ~= DRAM throughput: exceeds L3, hitting L3 misses

### Step 5: Optimization

Based on root cause:

**If memory-bound (utilization ~80%+):**
- You're near hardware limits, diminishing returns
- May need algorithmic changes

**If L1 cache issue:**
- Reduce working set
- Improve prefetching
- Reorder operations

**If L2 cache issue:**
- Check for non-sequential access
- Increase data reuse
- Use SIMD to reduce bandwidth

**If L3 cache issue:**
- Consider tiling/blocking algorithms
- Reduce working set
- Prefetch next block

**If compute-bound (utilization <30%):**
- Increase instruction-level parallelism
- Reduce data dependencies
- SIMD optimization
- Reduce branch misprediction

## Benchmark Descriptions

### cache_throughput_benchmark

**Purpose**: Establish hardware ceiling for memory bandwidth

**What it measures**:
- Sequential load throughput (GB/s)
- Sequential store throughput (GB/s)
- Mixed load/store throughput (GB/s)
- At each cache level (L1, L2, L3, DRAM)

**Key characteristics**:
- Stride-1 (sequential) access patterns
- Loop-unrolled for ILP
- Uses volatile/barriers to prevent optimization
- Single-threaded, no contention

**When to use**: First step - establish baseline

**Example output**:
```
Cache_Load_L1       148 ns     GB/s=103.452Gi/s
Cache_Load_L2      2999 ns     GB/s=81.4065Gi/s
Cache_Load_L3    101979 ns     GB/s=76.6307Gi/s
Cache_Load_DRAM 1232345 ns     GB/s=50.724Gi/s

Cache_Store_L1      96.7 ns    GB/s=157.778Gi/s
Cache_Store_L2     1543 ns     GB/s=158.199Gi/s
Cache_Store_L3    59425 ns     GB/s=131.482Gi/s
Cache_Store_DRAM 2443904 ns    GB/s=25.598Gi/s

Cache_Mixed_L1      50.1 ns    GB/s=304.584Gi/s
Cache_Mixed_L2     1573 ns     GB/s=155.226Gi/s
Cache_Mixed_L3    60315 ns     GB/s=129.647Gi/s
Cache_Mixed_DRAM 1920956 ns    GB/s=32.5525Gi/s
```

### column_benchmark

**Purpose**: Measure performance of column operations

**What it measures**:
- Element-wise operations (add, multiply, etc.)
- Operations on different data types (int32, float32, bf16)
- Operations on different column sizes
- Throughput in GB/s

**Key characteristics**:
- Uses actual Franklin column structures
- Includes actual compute (not just memory)
- Real-world access patterns
- SIMD optimized

**When to use**: After establishing cache baseline

**Analysis approach**:
1. Identify working set size from column size
2. Find matching cache level from baseline
3. Compare throughput
4. Calculate utilization

**Example**:
```bash
bazel run //benchmarks:column_benchmark -- \
  --benchmark_filter="BM_Int32_Add" \
  --benchmark_min_time=3s
```

### column_reduction_benchmark

**Purpose**: Measure performance of reduction operations

**What it measures**:
- Sum, product, min/max reductions
- On different column types
- On different column sizes
- Both scalar and vectorized implementations

**Key characteristics**:
- More compute-intensive than element-wise ops
- Lower bandwidth requirements
- May be less memory-bound

**Analysis approach**:
Same as column_benchmark, but look for:
- Computation overhead
- Pipeline efficiency
- Vector utilization

### kernel_fusion_benchmark

**Purpose**: Measure impact of loop fusion on memory traffic

**What it measures**:
- Naive approach: separate loops, multiple passes through data
- Fused approach: single loop, fewer memory passes
- Impact on throughput and cache utilization

**Key characteristics**:
- Demonstrates importance of algorithmic efficiency
- Memory-bound workload

**Analysis approach**:
- Compare fused vs naive throughput
- Should see improvement from:
  - Fewer memory reads
  - Better data reuse
  - Reduced cache pressure

## Performance Metrics Glossary

### GB/s (Gigabytes per second)
Primary throughput metric. Higher is better.

### Utilization %
`(measured_throughput / theoretical_maximum) * 100%`

Shows how efficiently you're using available bandwidth.

### IPC (Instructions Per Cycle)
From `perf stat`. Indicates pipeline efficiency.
- <1: Pipeline stalls, data dependencies
- 1-2: Normal scalar code
- >2: Good SIMD/ILP

### L1 miss rate
From `perf stat: cache-misses` / `cache-references`

Indicates working set doesn't fit L1 or access pattern is poor.

### Branch misprediction rate
From `perf stat: branch-misses` / `branches`

Indicates branch prediction failure, causing pipeline flushes.

### Cycles per instruction (CPI)
`cycles / instructions`. Inverse of IPC.

Lower is better.

## Tools for Deeper Analysis

### Linux perf
```bash
# Collect cache metrics
perf stat -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
  ./your_benchmark

# Detailed cache analysis
perf record -e L1-dcache-load-misses,L1-dcache-stores,LLC-loads,LLC-load-misses \
  ./your_benchmark
perf report
```

### Intel VTune
```bash
vtune -c memory-access ./your_benchmark
vtune -c microarchitecture ./your_benchmark
```

### Cachegrind (Valgrind)
```bash
valgrind --tool=cachegrind ./your_benchmark
cg_annotate cachegrind.out.XXXXX > analysis.txt
```

## Connecting Cache Theory to Implementation

### Why Sequential Access is Fast

Stride-1 (sequential) access triggers hardware prefetching:
- CPU detects pattern: address += 64 bytes each iteration
- Prefetcher automatically loads next cache line into L1
- Next iteration hits L1, no stall

**Bandwidth achieved**: L1 throughput (80-120 GB/s)

### Why Random Access is Slow

Random access defeats prefetching:
- CPU can't detect pattern
- No automatic prefetch
- Every load misses lower caches
- Stalls until data arrives from DRAM

**Bandwidth achieved**: ~5% of L3 throughput

### Why Loop Unrolling Matters

Without unrolling:
```cpp
for (i = 0; i < n; ++i) {
    result[i] = a[i] + b[i];  // Load A, load B, store (3 ops)
}
```
- Instruction 1 stalls waiting for load completion (~40ns)
- Can only do 1 operation every 40ns
- Bandwidth: ~1 operation / 40ns = ~250MB/s

With 8-way unrolling:
```cpp
for (i = 0; i < n; i += 8) {
    r1 = a[i];     // Load A[0]
    r2 = a[i+1];   // Load A[1]
    r3 = a[i+2];   // Load A[2]
    ...           // Meanwhile, A[0] has arrived
}
```
- CPU keeps 8 loads in flight
- Hides latency across multiple operations
- Bandwidth: ~8 operations / 5ns = ~50 GB/s

## Sample Analysis Report

### Operation: Int32 Column Sum Reduction

**Baseline Measurements** (cache_throughput_benchmark):
```
Cache_Mixed_L3: 120 GB/s (best case for mixed operations)
```

**Application Performance** (column_reduction_benchmark):
```
Input: 1M int32 elements = 4 MB
BM_Int32_Sum: time=5000ns, throughput=25 GB/s
```

**Analysis**:
```
Working set size: 4 MB (L3 resident)
Utilization: 25 GB/s / 120 GB/s = 20.8%

Interpretation:
- Not memory-bound (20% < 50% threshold)
- Limited by computation or pipeline stalls
- Reduction is inherently sequential (dependencies)
- No ILP due to data dependencies

Optimization opportunity:
- Use tree reduction for parallelism
- Or: Accept that this operation is compute-bound
```

## Best Practices

1. **Always start with cache_throughput_benchmark**: Know your hardware limits before optimizing
2. **Profile before optimizing**: Use perf to find real bottleneck
3. **Measure utilization**: Not just throughput in GB/s
4. **Consider data dependencies**: Reductions and other sequential ops are inherently limited
5. **Test on target hardware**: CPU frequency, cache sizes vary
6. **Run multiple times**: Account for system variance and throttling
7. **Use --benchmark_min_time=3s+**: Get accurate measurements
8. **Compare like-for-like**: L1 vs L1, L3 vs L3 (same working set size)

## Files

| File | Purpose |
|------|---------|
| `cache_throughput_benchmark.cpp` | Hardware baseline implementation |
| `CACHE_THROUGHPUT_METHODOLOGY.md` | Detailed methodology explanation |
| `CACHE_THROUGHPUT_QUICK_START.md` | Quick reference guide |
| `BENCHMARK_ANALYSIS_FRAMEWORK.md` | This file - complete analysis guide |
| `column_benchmark.cpp` | Application benchmarks |
| `column_reduction_benchmark.cpp` | Reduction operation benchmarks |
| `kernel_fusion_benchmark.cpp` | Loop fusion comparison |
| `BUILD` | Bazel configuration for all benchmarks |

## Example Workflow

```bash
# 1. Establish baseline
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_min_time=3s > baseline.txt

# 2. Benchmark application
bazel run //benchmarks:column_benchmark -- --benchmark_min_time=3s > app_results.txt

# 3. Analyze with perf
perf stat -e cycles,instructions,cache-references,cache-misses \
  bazel run //benchmarks:column_benchmark -- BM_Int32_Add

# 4. Interpret results:
#    - Compare app throughput vs baseline
#    - Check cache-miss rate from perf
#    - Decide on optimization strategy
```

## Additional Resources

- **CACHE_THROUGHPUT_METHODOLOGY.md** - Deep dive into benchmark design
- **CACHE_THROUGHPUT_QUICK_START.md** - Quick reference for common tasks
- **Google Benchmark documentation** - https://github.com/google/benchmark
- **Linux perf documentation** - `man perf`
- **"What Every Programmer Should Know About Memory"** - Ulrich Drepper
- **"Systems Performance"** - Brendan Gregg

---

For detailed methodology, see `CACHE_THROUGHPUT_METHODOLOGY.md`.
For quick reference, see `CACHE_THROUGHPUT_QUICK_START.md`.
