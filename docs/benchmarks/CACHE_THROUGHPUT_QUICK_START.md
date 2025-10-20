# Cache Throughput Benchmark - Quick Start Guide

## What This Benchmark Measures

The maximum **bandwidth** (GB/s) available for memory operations at each cache level:
- L1 cache (32 KB per core)
- L2 cache (1 MB per core)
- L3 cache (32 MB shared)
- Main memory (DRAM)

For each level, it measures:
- **Load**: Read-only sequential access
- **Store**: Write-only sequential access
- **Mixed**: Balanced read/write operations

## Run the Benchmark

### Quick Run (30 seconds total)
```bash
bazel run //benchmarks:cache_throughput_benchmark
```

### Accurate Run (2 minutes, better statistics)
```bash
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_min_time=3s
```

### Run Specific Tests
```bash
# Only L1 cache tests
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_filter=".*L1"

# Only load benchmarks
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_filter=".*Load.*"

# Only store benchmarks
bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_filter=".*Store.*"
```

## Read the Output

```
Benchmark                 Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------
Cache_Load_L1           148 ns          147 ns     18997846 GB/s=103.452Gi/s
```

**Key metric: `GB/s=103.452Gi/s`**

This means L1 can sustain 103 GB/s for sequential reads.

## Typical Results

On modern Intel/AMD CPUs:

| Operation | L1 | L2 | L3 | DRAM |
|-----------|----|----|----|----|
| Load | 80-120 GB/s | 60-90 GB/s | 40-70 GB/s | 30-60 GB/s |
| Store | 100-160 GB/s | 80-160 GB/s | 80-140 GB/s | 20-40 GB/s |
| Mixed | 150-300 GB/s | 120-180 GB/s | 100-150 GB/s | 40-70 GB/s |

(The mixed operations often exceed individual limits due to dual-issue capability.)

## How to Use These Results

### Analyze Your Column Operations

Get throughput of your benchmark:
```bash
bazel run //benchmarks:column_benchmark -- --benchmark_filter="BM_Int32_Add"
```

Look for `GB/s=X` in output.

### Check Utilization

```
Your operation throughput / Cache baseline = Utilization %

Example:
- Your operation: 25 GB/s
- L3 baseline: 60 GB/s
- Utilization: 25/60 = 42%
```

### Interpret Results

| Utilization | Meaning |
|-------------|---------|
| >80% | Excellent - you're near hardware limits |
| 50-80% | Good - reasonable efficiency |
| 20-50% | Fair - potential for optimization |
| <20% | Poor - significant overhead or wrong cache level |

### Find Bottlenecks

**If throughput drops from L1 to L2:**
- You have L1 cache misses
- Reduce working set or improve access patterns

**If throughput drops from L2 to L3:**
- You have L2 cache misses
- Check for stride or random access patterns

**If throughput drops from L3 to DRAM:**
- Working set exceeds L3
- Consider partitioning algorithm or tiling

## Implementation Details

- **Working sets**: 16KB (L1), 256KB (L2), 8MB (L3), 64MB (DRAM)
- **Access pattern**: Sequential (stride-1) for max throughput
- **ILP optimization**: 8-way loop unrolling
- **Compiler barriers**: Prevent optimization dead-code elimination
- **Measurement method**: Google Benchmark's SetBytesProcessed

## Files

| File | Purpose |
|------|---------|
| `cache_throughput_benchmark.cpp` | Implementation |
| `CACHE_THROUGHPUT_METHODOLOGY.md` | Detailed explanation |
| `CACHE_THROUGHPUT_QUICK_START.md` | This file |
| `BUILD` | Bazel configuration |

## Common Issues

### "WARNING: Library was built as DEBUG"
The benchmark is less accurate in debug builds. Rebuild with:
```bash
bazel build -c opt //benchmarks:cache_throughput_benchmark
```

### Results vary between runs
This is normal - CPU frequency scaling, thermal throttling, and system load cause variance. Run with `--benchmark_min_time=5s` for better averaging.

### L1 bandwidth seems low
Check if working set fits:
- L1 benchmark uses 16 KB
- Typical L1 per core: 32 KB
- Leave room for instruction cache and alignment

### Store throughput exceeds load throughput
This is normal and expected. Write operations can be buffered more efficiently than reads.

## Next Steps

1. **Establish baseline**: Run all cache levels on your target machine
2. **Profile your code**: Use `perf` or VTune to measure cache misses
3. **Compare**: Is your operation near the theoretical maximum?
4. **Optimize**: If underutilizing, look for cache misses, stalls, or poor ILP

See `CACHE_THROUGHPUT_METHODOLOGY.md` for deeper analysis.
