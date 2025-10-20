# Performance Analysis with perf and cache_throughput_benchmark

## Overview

This guide explains how to use Linux `perf` to validate and deepen analysis of cache throughput measurements and identify performance bottlenecks in Franklin benchmarks.

## Prerequisites

```bash
# Install perf
sudo apt-get install linux-tools

# Grant permissions (once)
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid

# Or run perf with sudo (every time)
sudo perf ...
```

## Quick Reference: Common perf Commands

### Basic Statistics
```bash
perf stat ./your_benchmark
```

Shows total cycles, instructions, cache references, cache misses.

### Detailed Cache Analysis
```bash
perf stat -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
  ./your_benchmark
```

### Record Events for Later Analysis
```bash
perf record -F 99 ./your_benchmark
perf report
```

### Top Hot Spots (Flame Graph)
```bash
perf record -F 99 -g ./your_benchmark
perf script | stackcollapse-perf.pl | flamegraph.pl > perf.svg
```

## Analysis Workflow

### Step 1: Run cache_throughput_benchmark with perf

Establish the cache baseline with performance counter data:

```bash
perf stat -e cycles,instructions,L1-dcache-load-misses,L1-dcache-store-misses,LLC-load-misses,LLC-store-misses \
  bazel run //benchmarks:cache_throughput_benchmark -- --benchmark_min_time=3s
```

Record output. Key metrics:
- **Cache misses**: Should be minimal for L1 (sequential access)
- **Instructions per cycle (IPC)**:
  - Formula: instructions / cycles
  - Expected: 2-4 (due to loop unrolling)

### Step 2: Run Application Benchmark with perf

```bash
perf stat -e cycles,instructions,cache-references,cache-misses \
  bazel run //benchmarks:column_benchmark -- --benchmark_min_time=3s BM_Int32_Add
```

Record:
- Total time
- Cache misses / cache references ratio
- IPC (instructions per cycle)

### Step 3: Calculate Key Metrics

#### Memory Footprint

```bash
# Get column size from benchmark
# E.g., BM_Int32_Add benchmarks sizes 1K to 100M elements

# For 10M element test:
# Working set = 10M elements * 4 bytes * 3 columns (2 read + 1 write) = 120 MB
# This exceeds L3 (32 MB), so expect DRAM throughput

working_set_mb = elements * element_size * num_columns / 1_000_000
```

#### Estimated Vs. Actual Throughput

```bash
# From cache_throughput_benchmark baseline:
# Cache_Load_L3: 76.6 GB/s (for L3-resident data)

# From column_benchmark:
# BM_Int32_Add_10M: 45 GB/s

# Utilization = 45 / 76.6 = 58.8%
# This is reasonable (not memory-bound)
```

#### Cache Miss Analysis

```
miss_rate = cache-misses / cache-references

If miss_rate > 10%:
  - Check if working set fits in target cache
  - Look for non-sequential access patterns
  - Profile with perf record for hot spots

If miss_rate < 2%:
  - Working set fits in cache
  - Access patterns are cache-friendly
  - Optimization should focus on compute, not memory
```

### Step 4: Identify Bottlenecks

Use this decision tree:

```
┌─ Is IPC > 2.0?
│  ├─ YES: Good instruction-level parallelism
│  │  └─ Bottleneck is NOT compute (may be memory or other)
│  └─ NO: Poor instruction parallelism
│     └─ Bottleneck: data dependencies or branch misprediction
│
└─ Is cache-miss rate > 5%?
   ├─ YES: Significant cache misses
   │  └─ Bottleneck: memory access pattern or working set too large
   └─ NO: Low cache misses
      └─ If throughput < 80% baseline: Bottleneck is compute/pipeline
```

## Detailed Examples

### Example 1: Memory-Bound Operation

**Setup**: Large column reduction where working set exceeds L3

**cache_throughput_benchmark output**:
```
Cache_Load_DRAM: 50 GB/s
Cache_Mixed_DRAM: 35 GB/s
```

**column_reduction_benchmark with perf**:
```
Performance counter stats:
      5,000 ms  time elapsed
  1,250,000 M  instructions           # 1.25B instructions in 5 seconds
    10,000 M  cycles                  # 10B cycles
    100,000 M  cache-misses
     500,000 M  cache-references      # 20% miss rate

IPC = 1.25B / 10B = 0.125 (LOW)
Miss rate = 100M / 500M = 20% (HIGH)
```

**Analysis**:
- Low IPC (0.125) indicates instruction-bound, not memory-bound
- High miss rate (20%) indicates data not in cache
- Reduction operations are inherently sequential (dependencies)
- **Conclusion**: Compute-bound, not memory optimization opportunity

### Example 2: Memory-Bound Operation

**Setup**: Element-wise operation on large dataset

**cache_throughput_benchmark output**:
```
Cache_Load_L3: 76 GB/s
Cache_Mixed_L3: 130 GB/s
```

**column_benchmark with perf**:
```
Performance counter stats:
    10,000 ms  time elapsed
  2,000,000 M  instructions          # 2B instructions
    10,000 M  cycles                 # 10B cycles
     50,000 M  cache-misses
    500,000 M  cache-references      # 10% miss rate

IPC = 2B / 10B = 0.2 (LOW)
Miss rate = 50M / 500M = 10% (MODERATE)
Throughput = 120 GB/s (from benchmark output)
```

**Analysis**:
- Low IPC despite moderate miss rate
- Throughput (120 GB/s) is near L3 baseline (130 GB/s)
- **Conclusion**: Memory-bound on L3, achieving 92% utilization
- **Optimization**: Can't improve further without algorithmic change

### Example 3: Compute-Bound Operation

**Setup**: Loop with heavy computation

**cache_throughput_benchmark output**:
```
Cache_Mixed_L1: 304 GB/s (theoretical max with compute)
```

**custom_benchmark with perf**:
```
Performance counter stats:
     5,000 ms  time elapsed
 10,000,000 M  instructions          # 10B instructions (high compute)
    10,000 M  cycles
      1,000 M  cache-misses
    100,000 M  cache-references      # 1% miss rate (low)

IPC = 10B / 10B = 1.0 (REASONABLE)
Miss rate = 1M / 100M = 1% (LOW)
Throughput = 50 GB/s (from benchmark output)
```

**Analysis**:
- Low miss rate indicates memory is not problem
- Moderate IPC (1.0) is expected for scalar operations
- Throughput (50 GB/s) is >> L1 load baseline (103 GB/s)
- **Why?** Because compute is overlapping with memory
- **Conclusion**: Compute-bound, memory ops are easily satisfied

## Reading perf stat Output

### Standard Output Format

```
Performance counter stats for 'bazel run ...':

    1,234,567 ms  context-switches          # 0.123 M/sec
       12,345 ms  cpu-migrations            # 0.001 M/sec
      123,456      page-faults              # 0.012 M/sec
  10,000,000,000  cycles                    # 3.333 GHz
  20,000,000,000  instructions              # 2.00  insn per cycle
   1,000,000,000  cache-references
      50,000,000  cache-misses              # 5.00% of all cache refs

       3.000000000 seconds time elapsed
```

### Key Metrics

| Metric | Good | Warning | Bad |
|--------|------|---------|-----|
| IPC | >2.0 | 1.0-2.0 | <1.0 |
| Cache miss % | <2% | 2-10% | >10% |
| Context switches | <1M | 1-10M | >10M |
| Page faults | <1K | 1-10K | >10K |

## Advanced Analysis: Cache Level Breakdown

Get detailed cache metrics at each level:

```bash
perf stat -e \
  L1-dcache-loads,L1-dcache-load-misses,\
  L1-dcache-stores,L1-dcache-store-misses,\
  LLC-loads,LLC-load-misses,\
  LLC-stores,LLC-store-misses,\
  memory-loads,memory-stores \
  ./your_benchmark
```

Interpreting:
- **L1-dcache-load-misses**: Data didn't fit in L1
- **LLC-load-misses**: Data didn't fit in L3 (went to DRAM)
- **memory-loads/stores**: Main memory operations

## Building Flame Graphs

For visual identification of hot spots:

```bash
# 1. Install tools
sudo apt-get install linux-tools git
git clone https://github.com/brendangregg/FlameGraph.git

# 2. Record
perf record -F 99 -g bazel run //benchmarks:column_benchmark

# 3. Generate
perf script | ./FlameGraph/stackcollapse-perf.pl | \
  ./FlameGraph/flamegraph.pl > perf.svg

# 4. View
firefox perf.svg
```

Hot spots in flame graph:
- Tall frames = functions taking CPU time
- Width = how often called
- Look for wide stacks to find optimization opportunities

## Integrating with cache_throughput_benchmark

### Validation Workflow

```bash
# Step 1: Establish baseline with detailed metrics
perf stat -e cycles,instructions,cache-references,cache-misses \
  bazel run //benchmarks:cache_throughput_benchmark -- \
  --benchmark_filter="Cache_Mixed_L3" > cache_baseline.txt

# Step 2: Profile your operation
perf stat -e cycles,instructions,cache-references,cache-misses \
  bazel run //benchmarks:column_benchmark -- \
  --benchmark_filter="Your_Operation" > app_profile.txt

# Step 3: Compare
grep "GHz\|insn per cycle\|cache refs\|cache misses" cache_baseline.txt
grep "GHz\|insn per cycle\|cache refs\|cache misses" app_profile.txt

# Calculate ratios and determine bottleneck
```

### Automated Analysis Script

```python
#!/usr/bin/env python3
import subprocess
import re

def run_perf(benchmark_filter):
    """Run benchmark with perf and parse output."""
    cmd = f"""
    perf stat -e cycles,instructions,cache-references,cache-misses \
      bazel run //benchmarks:column_benchmark -- \
      --benchmark_filter="{benchmark_filter}"
    """
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    # Parse output
    cycles = int(re.search(r'(\d+)\s+cycles', result.stdout).group(1))
    instructions = int(re.search(r'(\d+)\s+instructions', result.stdout).group(1))
    cache_misses = int(re.search(r'(\d+)\s+cache-misses', result.stdout).group(1))
    cache_refs = int(re.search(r'(\d+)\s+cache-references', result.stdout).group(1))

    ipc = instructions / cycles if cycles > 0 else 0
    miss_rate = (cache_misses / cache_refs * 100) if cache_refs > 0 else 0

    return {
        'ipc': ipc,
        'miss_rate': miss_rate,
        'cycles': cycles,
        'instructions': instructions,
    }

# Run analysis
baseline = run_perf("Cache_Mixed_L3")
app = run_perf("BM_Int32_Add")

print(f"IPC (cache baseline): {baseline['ipc']:.2f}")
print(f"IPC (application): {app['ipc']:.2f}")
print(f"Cache miss rate: {app['miss_rate']:.1f}%")

if baseline['ipc'] > 2.0 and app['miss_rate'] < 5:
    print("Compute-bound: focus on IPC")
elif app['miss_rate'] > 10:
    print("Memory-bound with misses: optimize access pattern")
else:
    print("Memory-bound, cache-friendly: nearing hardware limits")
```

## Troubleshooting

### "Permission denied" when running perf
```bash
# Either:
sudo perf stat ...

# Or fix permissions (one-time):
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
```

### "event 'cycles' not found"
Your CPU may not support this event. Use:
```bash
perf list
```

### High variance in results
- Use `--benchmark_min_time=5s` or more
- Close other applications
- Disable CPU frequency scaling: `sudo cpupower frequency-set -g performance`

### Can't see certain events
Some events require:
- Kernel compiled with `CONFIG_PERF_EVENTS=y`
- CPU that supports that event
- Intel CPUs need `CONFIG_INTEL_RAPL=y` for power events

## Best Practices

1. **Always establish baseline first**: Run cache_throughput_benchmark before optimizing
2. **Use consistent settings**: Same CPU governor, same core affinity
3. **Run multiple times**: Account for system variance
4. **Profile before optimizing**: Know the real bottleneck
5. **Check IPC first**: Quick indicator of pipeline efficiency
6. **Measure miss rates**: Key for memory optimization
7. **Use --benchmark_min_time=3s+**: Get statistical significance
8. **Record flame graphs**: Visual analysis of hot spots

## References

- Linux perf tutorial: https://perf.wiki.kernel.org/index.php/Tutorial
- Brendan Gregg's perf examples: http://www.brendangregg.com/perf.html
- Google Benchmark guide: https://github.com/google/benchmark
- Intel VTune alternative: https://www.intel.com/content/www/us/en/develop/documentation/vtune-cookbook/top.html

---

For more details on cache_throughput_benchmark methodology, see `CACHE_THROUGHPUT_METHODOLOGY.md`.
