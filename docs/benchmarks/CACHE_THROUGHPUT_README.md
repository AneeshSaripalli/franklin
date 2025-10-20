# Cache Throughput Benchmark

## Summary

The cache throughput benchmark empirically measures the maximum load/store bandwidth available at each level of the CPU cache hierarchy on the Franklin target machine. This provides a hardware-accurate ceiling for performance analysis of memory-intensive operations like column data processing.

## Quick Start

### Run the Benchmark

```bash
# Basic run (~30 seconds)
bazel run //benchmarks:cache_throughput_benchmark

# Accurate run with better statistics (~2 minutes)
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_min_time=3s

# Run only L1 tests
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_filter=".*L1"
```

### Sample Results

```
Benchmark                 Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------
Cache_Load_L1           149 ns          149 ns     18937083 GB/s=102.68Gi/s
Cache_Load_L2          3036 ns         3035 ns       930148 GB/s=80.4316Gi/s
Cache_Load_L3        103013 ns       102975 ns        27185 GB/s=75.8678Gi/s
Cache_Load_DRAM     1241216 ns      1241027 ns         2244 GB/s=50.3615Gi/s
Cache_Store_L1         96.7 ns         96.7 ns     28835279 GB/s=157.767Gi/s
Cache_Store_L2         1561 ns         1561 ns      1801030 GB/s=156.409Gi/s
Cache_Store_L3        60212 ns        60172 ns        46325 GB/s=129.836Gi/s
Cache_Store_DRAM    2537358 ns      2534827 ns         1007 GB/s=24.6565Gi/s
Cache_Mixed_L1         50.1 ns         50.0 ns     55671889 GB/s=304.892Gi/s
Cache_Mixed_L2         1564 ns         1564 ns      1787975 GB/s=156.103Gi/s
Cache_Mixed_L3        59516 ns        59511 ns        47090 GB/s=131.278Gi/s
Cache_Mixed_DRAM    2048271 ns      2047966 ns         1450 GB/s=30.5181Gi/s
```

## What It Measures

The benchmark measures **throughput** (GB/s) for three operation types at each cache level:

| Operation | What | Use Case |
|-----------|------|----------|
| **Load** | Sequential read-only access | Read-heavy operations |
| **Store** | Sequential write-only access | Write-heavy operations |
| **Mixed** | Balanced read/write access | Typical data transformation |

At each cache level:

| Level | Size | Working Set | Purpose |
|-------|------|-------------|---------|
| **L1** | 32 KB | 16 KB | Fastest (register-like speed) |
| **L2** | 1 MB | 256 KB | Per-core cache |
| **L3** | 32 MB | 8 MB | Shared, larger |
| **DRAM** | GBs | 64 MB | Main memory |

## Key Results

On this system (Intel Skylake-SP like):

### Load Throughput (Read-Only)
- L1: 102 GB/s (best case, all reads from register)
- L2: 80 GB/s (small penalty)
- L3: 75 GB/s (larger penalty, shared resource)
- DRAM: 50 GB/s (main memory bandwidth)

### Store Throughput (Write-Only)
- L1: 157 GB/s (write-combining buffer efficient)
- L2: 156 GB/s (maintains efficiency)
- L3: 129 GB/s (still good)
- DRAM: 24 GB/s (write-back bottleneck)

### Mixed Load/Store
- L1: 304 GB/s (load and store units can run in parallel)
- L2: 156 GB/s (slightly less than store alone)
- L3: 131 GB/s (similar to store pattern)
- DRAM: 30 GB/s (dual-issue helps slightly)

## Why This Matters

When optimizing Franklin column operations, you need to know:

1. **Can you achieve measured throughput?** Compare column operation throughput against these baselines
2. **Where's the bottleneck?** If you achieve 40% of L1 throughput on data that fits in L1, it's not memory
3. **Is optimization worth it?** If you're at 80%+ of theoretical maximum, you've hit hardware limits

## Using These Results

### Calculate Utilization

```
Utilization = (Operation Throughput) / (Cache Level Throughput) * 100%

Example:
- Your operation: 60 GB/s
- L3 baseline: 75 GB/s
- Utilization: 60/75 = 80% (very good)
```

### Identify Cache Level

Working set size → matches cache level:

```
- < 16 KB → L1 (compare against L1 baseline)
- < 256 KB → L2 (compare against L2 baseline)
- < 8 MB → L3 (compare against L3 baseline)
- > 8 MB → DRAM (compare against DRAM baseline)
```

### Detect Problems

| Symptom | Likely Cause |
|---------|-------------|
| Throughput 50% of L1 baseline on L1-resident data | Compute-bound, not memory |
| Throughput drops to L2 when data fits in L1 | L1 cache misses (access pattern) |
| Throughput drops to L3 when data fits in L2 | L2 cache misses |
| Linear drop from L3 to DRAM | Prefetching failing, data larger than L3 |

## Architecture

### Benchmark Structure

```
cache_throughput_benchmark.cpp
├── Sequential Load Tests
│   ├── BM_Sequential_Load_L1     (16 KB working set)
│   ├── BM_Sequential_Load_L2     (256 KB working set)
│   ├── BM_Sequential_Load_L3     (8 MB working set)
│   └── BM_Sequential_Load_DRAM   (64 MB working set)
├── Sequential Store Tests
│   ├── BM_Sequential_Store_L1
│   ├── BM_Sequential_Store_L2
│   ├── BM_Sequential_Store_L3
│   └── BM_Sequential_Store_DRAM
└── Mixed Load/Store Tests
    ├── BM_Mixed_LoadStore_L1
    ├── BM_Mixed_LoadStore_L2
    ├── BM_Mixed_LoadStore_L3
    └── BM_Mixed_LoadStore_DRAM
```

### Key Design Choices

1. **Sequential Access** (stride-1)
   - Triggers hardware prefetching
   - Measures throughput, not latency
   - Best-case scenario for each cache

2. **Loop Unrolling** (8-way)
   - Enables instruction-level parallelism (ILP)
   - CPU keeps 8 operations in flight
   - Removes latency hiding

3. **Compiler Barriers**
   - Prevents dead-code elimination
   - Prevents unwanted optimization
   - Ensures real memory operations

4. **Volatile Barriers**
   - `asm volatile("" ::: "memory")`
   - Tells compiler memory might change
   - Prevents reordering

## Build Configuration

```bazel
cc_binary(
    name = "cache_throughput_benchmark",
    srcs = ["cache_throughput_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-O3",           # Maximum optimization
        "-march=native", # Use CPU features (AVX, etc.)
    ],
    deps = [
        "@google_benchmark//:benchmark",
    ],
)
```

## Files Provided

| File | Purpose |
|------|---------|
| `cache_throughput_benchmark.cpp` | Implementation (565 lines) |
| `CACHE_THROUGHPUT_METHODOLOGY.md` | Detailed methodology and theory |
| `CACHE_THROUGHPUT_QUICK_START.md` | Quick reference guide |
| `CACHE_THROUGHPUT_README.md` | This file - overview |
| `BENCHMARK_ANALYSIS_FRAMEWORK.md` | Complete analysis workflow |
| `PERF_ANALYSIS_GUIDE.md` | Using Linux perf for validation |
| `BUILD` | Bazel integration (already updated) |

## Methodology Details

See `CACHE_THROUGHPUT_METHODOLOGY.md` for:
- Detailed explanation of working set sizes
- Why each design choice matters
- How to interpret results
- Limitations and caveats
- References and further reading

## Quick Reference

See `CACHE_THROUGHPUT_QUICK_START.md` for:
- Common command-line options
- How to read output
- Typical results on different hardware
- Troubleshooting common issues

## Performance Analysis Workflow

See `BENCHMARK_ANALYSIS_FRAMEWORK.md` for:
- Step-by-step analysis workflow
- Connecting cache theory to implementation
- Example analysis reports
- Best practices

## Using perf for Validation

See `PERF_ANALYSIS_GUIDE.md` for:
- Running with Linux perf for validation
- Interpreting cache miss rates
- Identifying bottlenecks
- Building flame graphs
- Automated analysis scripts

## Integration with Other Benchmarks

### Column Operations

```bash
# 1. Run cache baseline
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_min_time=3s

# 2. Run column operation
bazel run //benchmarks:column_benchmark -- --benchmark_filter="BM_Int32_Add" --benchmark_min_time=3s

# 3. Compare throughput
# Extract GB/s from each, calculate utilization
```

### Kernel Fusion

```bash
# Measures impact of loop fusion on memory traffic
bazel run //benchmarks:kernel_fusion_benchmark -- --benchmark_min_time=3s

# Compare against cache_throughput_benchmark to see:
# - Bandwidth reduction from fusion
# - Impact on cache misses
```

### Column Reductions

```bash
# Reductions are inherently sequential (dependencies)
bazel run //benchmarks:column_reduction_benchmark -- --benchmark_min_time=3s

# Compare against cache_throughput_benchmark to see:
# - If compute or memory is limiting
# - Whether tree reduction would help
```

## Expected Performance on Typical Hardware

### Intel Skylake-SP (3.2 GHz)
- L1: 100-120 GB/s
- L2: 70-90 GB/s
- L3: 60-80 GB/s
- DRAM: 40-60 GB/s

### AMD Zen 3 (4.0 GHz)
- L1: 80-100 GB/s
- L2: 60-80 GB/s
- L3: 50-70 GB/s
- DRAM: 50-70 GB/s

### ARM Neoverse
- L1: 60-80 GB/s
- L2: 40-60 GB/s
- L3: 30-50 GB/s
- DRAM: 20-40 GB/s

(Your actual hardware results are above - use those for analysis)

## Limitations

1. **Single-threaded**: Doesn't measure multi-core behavior
2. **Sequential access**: Real workloads often have irregular patterns
3. **Synthetic**: No actual compute overhead
4. **Warm cache**: Doesn't measure cold-start behavior
5. **No contention**: No multi-threaded cache coherency effects

## Next Steps

1. **Establish baseline**: Run benchmark, record results
2. **Profile your code**: Use `perf stat` on column benchmarks
3. **Compare throughput**: Your operation vs baseline
4. **Calculate utilization**: (Your throughput / baseline) * 100%
5. **Interpret results**: See BENCHMARK_ANALYSIS_FRAMEWORK.md

## Command Reference

```bash
# Run all benchmarks, 3 seconds each
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_min_time=3s

# Run only mixed operations
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_filter=".*Mixed.*"

# Run only L3 tests
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_filter=".*L3"

# Save to CSV
bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_min_time=3s \
  --benchmark_out=results.csv \
  --benchmark_out_format=csv

# Multiple runs for statistics
bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_min_time=3s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true
```

## Troubleshooting

### Build fails with "undefined reference to main"
Make sure `BENCHMARK_MAIN();` is at end of file. Already included in provided code.

### Results vary wildly between runs
- Run with longer `--benchmark_min_time=5s`
- Close other applications
- Disable CPU frequency scaling if possible

### Library was built as DEBUG
Compile in Release mode:
```bash
bazel build -c opt //benchmarks:cache_throughput_benchmark
```

### Getting different numbers than expected
- Check CPU frequency (turbo boost varies)
- System load affects results
- Thermal throttling under load
- Different CPU generation

## Support Files

For deeper understanding:

- **CACHE_THROUGHPUT_METHODOLOGY.md**: 11 KB, explains every design choice
- **PERF_ANALYSIS_GUIDE.md**: 12 KB, shows how to validate results
- **BENCHMARK_ANALYSIS_FRAMEWORK.md**: 13 KB, complete analysis workflow
- **CACHE_THROUGHPUT_QUICK_START.md**: 4 KB, quick reference

All provided in `/home/remoteaccess/code/franklin/benchmarks/`

## Key Takeaways

1. **Cache throughput varies significantly by level** - from 300+ GB/s (L1 mixed) to 25 GB/s (DRAM store)

2. **Sequential access is crucial** - random access would achieve 5% of these numbers

3. **Write operations can be more efficient** - store throughput often exceeds load on L1-L3

4. **Mixed operations leverage both units** - load and store ports can be used simultaneously

5. **Use these results to understand your code** - if you're not near these numbers on matching working sets, there's a bottleneck elsewhere

---

**For detailed methodology**, see `CACHE_THROUGHPUT_METHODOLOGY.md`

**For quick reference**, see `CACHE_THROUGHPUT_QUICK_START.md`

**For complete analysis workflow**, see `BENCHMARK_ANALYSIS_FRAMEWORK.md`

**For perf integration**, see `PERF_ANALYSIS_GUIDE.md`
