# Cache Throughput Benchmark Methodology

## Overview

The cache throughput benchmark (`cache_throughput_benchmark.cpp`) empirically measures the maximum load/store bandwidth available at each cache level on the native architecture. This provides a crucial baseline for understanding the hardware capabilities that applications must respect when optimizing data structure operations.

## Motivation

When optimizing data processing code (like the column operations in `column_benchmark.cpp`), it is critical to understand:

1. **Hardware ceiling**: What is the theoretical maximum throughput we can achieve?
2. **Bandwidth utilization**: Are we achieving a reasonable fraction of available bandwidth?
3. **Cache hierarchy**: Where is the performance cliff (L1 to L2, L2 to L3, L3 to DRAM)?

The benchmark answers these questions by measuring sequential memory access patterns through each cache level.

## Benchmark Design

### Working Set Sizes

Each benchmark targets a specific cache level by choosing a working set that:
- **Fits entirely** in that cache and lower levels
- **Exceeds** higher-level caches to avoid their interference

| Cache Level | Size | Working Set | Fits In |
|-------------|------|-------------|---------|
| L1          | ~32 KB | 16 KB | L1 (with overhead) |
| L2          | ~1 MB | 256 KB | L2, exceeds L1 |
| L3          | ~32 MB | 8 MB | L3, exceeds L2 |
| DRAM        | GBs | 64 MB | DRAM, exceeds L3 |

### Access Pattern

**Sequential access** (stride-1) is used because:
1. Maximizes cache line utilization (64 bytes per line, only 8 bytes used)
2. Enables hardware prefetching to keep the cache pipeline full
3. Represents best-case throughput for each cache level
4. Simplifies analysis: we're measuring **bandwidth**, not latency

### Measurements

The benchmark measures three distinct patterns:

#### 1. Sequential Load (Read-Only)
```
for each element:
    dummy ^= ptr[i]
```
- Stresses the L1/L2/L3 load ports
- XOR prevents dead code elimination
- No write pressure, enables write-combining buffers to remain available

#### 2. Sequential Store (Write-Only)
```
for each element:
    ptr[i] = write_value
```
- Stresses the store ports and store buffers
- Many modern CPUs can sustain higher store bandwidth than load bandwidth
  (due to write-combining and speculative pre-allocation)

#### 3. Mixed Load/Store
```
for each iteration:
    load 4 elements
    store 4 elements
```
- Represents typical data transformation code
- Exercises both load and store bandwidth simultaneously
- More realistic than read/write-only patterns

### Instruction-Level Parallelism (ILP)

Each benchmark uses **loop unrolling** to maximize ILP:
```cpp
for (size_t i = 0; i < count; i += 8) {
    dummy ^= p[i];
    dummy ^= p[i+1];
    dummy ^= p[i+2];
    ...
    dummy ^= p[i+7];
}
```

This is crucial because modern CPUs can sustain 4-8 memory operations in flight. Without unrolling, you only measure latency of a single operation, not throughput.

### Compiler Barriers

Two barriers are used:

1. **Compiler Barrier** (`asm volatile("" ::: "memory")`):
   - Prevents the compiler from dead-code-eliminating memory operations
   - Prevents the compiler from optimizing multiple operations into fewer operations
   - Does NOT flush the CPU pipeline or memory hierarchy

2. **DoNotOptimize**:
   - Prevents the compiler from optimizing away results
   - Uses the value in a way that can't be predicted at compile time

These barriers ensure we measure **what actually happens at runtime**, not what the compiler thinks should happen.

## Hardware Capabilities (Typical Skylake-SP)

The benchmark measures real bandwidth on the target machine. Expected results on a typical Skylake-SP:

| Operation | L1 | L2 | L3 | DRAM |
|-----------|----|----|----|----|
| Load | 60-80 GB/s | 40-50 GB/s | 30-40 GB/s | 20-30 GB/s |
| Store | 40-60 GB/s | 40-50 GB/s | 30-40 GB/s | 20-30 GB/s |
| Mixed | 60-100 GB/s | 70-90 GB/s | 60-80 GB/s | 30-40 GB/s |

**Note**: The mixed load/store often exceeds individual load or store throughput because:
- Load and store operations use different execution units
- The CPU can overlap load and store execution
- This demonstrates the importance of balancing data flow

## Interpreting Results

### Throughput Ceiling

Compare your application's throughput against these baselines:

```
Application throughput (GB/s) / Cache level throughput (GB/s) = utilization %
```

If your application achieves 20 GB/s on operations that fit in L3, and L3 throughput is 40 GB/s, you're at 50% utilization. This suggests:
- Either compute is the bottleneck (good), or
- Memory access patterns are non-optimal (cache misses, poor prefetching)

### Cache Miss Detection

Compare results across cache levels:
- If L1 throughput ≈ L2 throughput, you're hitting L1 cache misses
- If L2 throughput ≈ L3 throughput, you're hitting L2 cache misses
- If L3 throughput ≈ DRAM throughput, you're hitting L3 cache misses

### Prefetching Effectiveness

If a workload achieves 50% of theoretical L1 throughput but working set fits in L1:
- Could indicate insufficient instruction-level parallelism
- Could indicate pipeline stalls (data dependencies)
- Suggests software prefetching or loop restructuring might help

## Running the Benchmark

### Basic Usage
```bash
bazel run //benchmarks:cache_throughput_benchmark
```

### With Options
```bash
bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_min_time=3s \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
```

### Capturing Output
```bash
bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_out_format=csv \
  --benchmark_out=cache_results.csv
```

### Filtering Benchmarks
```bash
# Run only L1 benchmarks
bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_filter="Cache.*L1"

# Run only load benchmarks
bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_filter=".*Load.*"
```

## Expected Output Format

```
Benchmark                 Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------
Cache_Load_L1           148 ns          147 ns     18997846 GB/s=103.452Gi/s
Cache_Load_L2          2999 ns         2999 ns       930261 GB/s=81.4065Gi/s
Cache_Load_L3        101979 ns       101950 ns        27847 GB/s=76.6307Gi/s
Cache_Load_DRAM     1232345 ns      1232158 ns         2369 GB/s=50.724Gi/s
Cache_Store_L1         96.7 ns         96.7 ns     29112804 GB/s=157.778Gi/s
Cache_Store_L2         1543 ns         1543 ns      1797776 GB/s=158.199Gi/s
Cache_Store_L3        59425 ns        59419 ns        48162 GB/s=131.482Gi/s
Cache_Store_DRAM    2443904 ns      2441598 ns         1151 GB/s=25.598Gi/s
Cache_Mixed_L1         50.1 ns         50.1 ns     56371870 GB/s=304.584Gi/s
Cache_Mixed_L2         1573 ns         1573 ns      1768316 GB/s=155.226Gi/s
Cache_Mixed_L3        60315 ns        60260 ns        46269 GB/s=129.647Gi/s
Cache_Mixed_DRAM    1920956 ns      1919977 ns         1480 GB/s=32.5525Gi/s
```

Key columns:
- **Time**: Wall-clock time per operation (nanoseconds)
- **CPU**: CPU time per operation (nanoseconds)
- **Iterations**: How many times the benchmark ran before averaging
- **GB/s**: Throughput in GB/s (the primary metric)

## Connecting to Application Benchmarks

Use these results to analyze `column_benchmark.cpp` and `column_reduction_benchmark.cpp`:

1. **Measure throughput** of column operations (using SetBytesProcessed)
2. **Identify working set size** by analyzing memory access patterns
3. **Compare against baseline** from cache_throughput_benchmark
4. **Calculate utilization** as a percentage

Example:
- Int32 add operation on 1M elements: 25 GB/s throughput
- Working set: 3 columns * 4M bytes = 12 MB (fits in L3)
- L3 baseline: 40 GB/s
- **Utilization: 62.5%** - this is reasonable for a simple operation

## Limitations

1. **Single-threaded**: Doesn't account for multi-core cache coherency overhead or NUMA effects
2. **Sequential access**: Real workloads often have irregular patterns (cache misses)
3. **Synthetic**: Doesn't include compute cycles that would disrupt prefetching
4. **Warm cache**: Doesn't measure cold-start penalties

For multi-threaded analysis, use:
- Linux `perf` to measure cache misses and coherency traffic
- VTune for detailed cache analysis
- Custom benchmarks with actual column structures

## Implementation Details

### File: `cache_throughput_benchmark.cpp`

Organized into three groups:
1. **Sequential Load**: 4 benchmarks (L1, L2, L3, DRAM)
2. **Sequential Store**: 4 benchmarks (L1, L2, L3, DRAM)
3. **Mixed Load/Store**: 4 benchmarks (L1, L2, L3, DRAM)

Each benchmark:
- Allocates a std::vector sized for the target cache level
- Fills with pseudo-random data (prevents cache compression)
- Measures throughput over multiple iterations
- Reports in GB/s using Google Benchmark's Counter facility

### Build Configuration

```bazel
cc_binary(
    name = "cache_throughput_benchmark",
    srcs = ["cache_throughput_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-O3",           # Maximum optimization
        "-march=native", # Use all native CPU features
    ],
    deps = [
        "@google_benchmark//:benchmark",
    ],
)
```

Key flags:
- `-O3`: Enables all optimizations (necessary for accurate throughput measurement)
- `-march=native`: Uses all available CPU features (SSE, AVX, AVX2, etc.)

## References

### Hardware Architecture
- Intel 64 and IA-32 Architectures Optimization Reference Manual
- Intel Software Optimization Guide
- "What Every Programmer Should Know About Memory" (Ulrich Drepper)

### Benchmarking Methodology
- "Benchmarking: Realistic Expectations and Cautious Tactics" (Dongarra & Dunigan)
- Google Benchmark documentation
- SPEC benchmarking practices

### Performance Analysis
- Linux perf documentation: `man perf`
- Intel VTune Profiler documentation
- "Systems Performance" (Brendan Gregg)

## Future Enhancements

Potential improvements to the benchmark:

1. **SIMD throughput**: Measure with AVX-256 and AVX-512 instructions
2. **Prefetch analysis**: Compare with/without explicit prefetching
3. **NUMA effects**: Measure on multi-socket systems
4. **Contention**: Measure cache coherency overhead with multiple threads
5. **Access stride**: Vary access patterns (stride-2, random, etc.)
6. **Write patterns**: Compare write-combining vs. write-through

These would require additional benchmark variations and analysis framework.
