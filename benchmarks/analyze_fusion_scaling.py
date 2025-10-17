#!/usr/bin/env python3
"""
Analyze kernel fusion scaling across cache hierarchy.

Parses benchmark output and shows how speedup changes from L1 to beyond L3.
"""

import re

# Benchmark results from optimized build (-c opt)
NAIVE_RESULTS = """
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/8192           1017 ns         1016 ns       699571 bytes_per_second=120.102Gi/s elements=8.192k size_kb=32
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/16384          1990 ns         1989 ns       348327 bytes_per_second=122.716Gi/s elements=16.384k size_kb=64
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/32768          4010 ns         4008 ns       167718 bytes_per_second=121.84Gi/s elements=32.768k size_kb=128
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/65536          9158 ns         9155 ns        77726 bytes_per_second=106.664Gi/s elements=65.536k size_kb=256
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/131072        18985 ns        18980 ns        37351 bytes_per_second=102.905Gi/s elements=131.072k size_kb=512
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/262144        37664 ns        37661 ns        18435 bytes_per_second=103.722Gi/s elements=262.144k size_kb=1.024k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/524288        75803 ns        75774 ns         9586 bytes_per_second=103.102Gi/s elements=524.288k size_kb=2.048k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/1048576      160630 ns       160602 ns         4372 bytes_per_second=97.2902Gi/s elements=1.04858M size_kb=4.096k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/2097152      574731 ns       574682 ns         1195 bytes_per_second=54.3779Gi/s elements=2.09715M size_kb=8.192k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/4194304     1813955 ns      1813775 ns          376 bytes_per_second=34.4585Gi/s elements=4.1943M size_kb=16.384k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/8388608     5014612 ns      5014134 ns          144 bytes_per_second=24.9295Gi/s elements=8.38861M size_kb=32.768k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/16777216   12610665 ns     12605235 ns           56 bytes_per_second=19.833Gi/s elements=16.7772M size_kb=65.536k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/33554432   25322114 ns     25321073 ns           28 bytes_per_second=19.7464Gi/s elements=33.5544M size_kb=131.072k
KernelFusionFixture/Naive_TwoLoops_MulThenAdd/67108864   50438557 ns     50405501 ns           14 bytes_per_second=19.8391Gi/s elements=67.1089M size_kb=262.144k
"""

FUSED_RESULTS = """
KernelFusionFixture/Fused_SingleLoop_FMA/8192                 786 ns          786 ns       882389 bytes_per_second=155.293Gi/s elements=8.192k size_kb=32
KernelFusionFixture/Fused_SingleLoop_FMA/16384               1568 ns         1568 ns       446459 bytes_per_second=155.739Gi/s elements=16.384k size_kb=64
KernelFusionFixture/Fused_SingleLoop_FMA/32768               3112 ns         3111 ns       225394 bytes_per_second=156.937Gi/s elements=32.768k size_kb=128
KernelFusionFixture/Fused_SingleLoop_FMA/65536               7336 ns         7335 ns        96231 bytes_per_second=133.134Gi/s elements=65.536k size_kb=256
KernelFusionFixture/Fused_SingleLoop_FMA/131072             14840 ns        14838 ns        46060 bytes_per_second=131.626Gi/s elements=131.072k size_kb=512
KernelFusionFixture/Fused_SingleLoop_FMA/262144             30165 ns        30162 ns        23303 bytes_per_second=129.507Gi/s elements=262.144k size_kb=1.024k
KernelFusionFixture/Fused_SingleLoop_FMA/524288             60836 ns        60815 ns        11548 bytes_per_second=128.464Gi/s elements=524.288k size_kb=2.048k
KernelFusionFixture/Fused_SingleLoop_FMA/1048576           155198 ns       155182 ns         5127 bytes_per_second=100.688Gi/s elements=1.04858M size_kb=4.096k
KernelFusionFixture/Fused_SingleLoop_FMA/2097152           465657 ns       465338 ns         1395 bytes_per_second=67.1555Gi/s elements=2.09715M size_kb=8.192k
KernelFusionFixture/Fused_SingleLoop_FMA/4194304          1522573 ns      1521751 ns          462 bytes_per_second=41.0711Gi/s elements=4.1943M size_kb=16.384k
KernelFusionFixture/Fused_SingleLoop_FMA/8388608          3914487 ns      3914173 ns          179 bytes_per_second=31.9352Gi/s elements=8.38861M size_kb=32.768k
KernelFusionFixture/Fused_SingleLoop_FMA/16777216         8813079 ns      8804237 ns           86 bytes_per_second=28.3954Gi/s elements=16.7772M size_kb=65.536k
KernelFusionFixture/Fused_SingleLoop_FMA/33554432        18415052 ns     18409476 ns           37 bytes_per_second=27.1599Gi/s elements=33.5544M size_kb=131.072k
KernelFusionFixture/Fused_SingleLoop_FMA/67108864        35367680 ns     35234225 ns           20 bytes_per_second=28.3815Gi/s elements=67.1089M size_kb=262.144k
"""


def parse_benchmark_line(line):
    """Parse a single benchmark result line."""
    match = re.search(r'/(\d+)\s+(\d+)\s+ns.*size_kb=([\d.k]+)', line)
    if match:
        elements = int(match.group(1))
        time_ns = int(match.group(2))
        size_kb_str = match.group(3)

        # Parse size (handle 'k' suffix)
        if 'k' in size_kb_str:
            size_kb = float(size_kb_str.replace('k', '')) * 1024
        else:
            size_kb = float(size_kb_str)

        return elements, time_ns, size_kb
    return None


def categorize_by_cache(size_kb):
    """Categorize array size by cache level."""
    if size_kb <= 32:
        return "L1 (32 KB)"
    elif size_kb <= 1024:
        return "L2 (1 MB)"
    elif size_kb <= 32768:
        return "L3 (32 MB)"
    else:
        return "DRAM (>L3)"


def main():
    # Parse results
    naive_data = {}
    for line in NAIVE_RESULTS.strip().split('\n'):
        result = parse_benchmark_line(line)
        if result:
            elements, time_ns, size_kb = result
            naive_data[elements] = {'time_ns': time_ns, 'size_kb': size_kb}

    fused_data = {}
    for line in FUSED_RESULTS.strip().split('\n'):
        result = parse_benchmark_line(line)
        if result:
            elements, time_ns, size_kb = result
            fused_data[elements] = {'time_ns': time_ns, 'size_kb': size_kb}

    # Calculate speedups
    print("=" * 100)
    print("KERNEL FUSION SCALING ANALYSIS: L1 Cache → DRAM")
    print("=" * 100)
    print()
    print("System Configuration:")
    print("  L1 Data:    32 KB (per core)")
    print("  L2 Unified: 1 MB (per core)")
    print("  L3 Unified: 32 MB (shared)")
    print()
    print("Benchmark Setup:")
    print("  - Naive: Two loops (mul then add) with AVX2, 6 memory ops/element")
    print("  - Fused: Single loop with FMA, 4 memory ops/element")
    print("  - Bandwidth: Measured as (3 inputs + 1 output) × 4 bytes = 16 bytes/element")
    print()
    print("-" * 100)
    print(f"{'Cache Level':<12} {'Size (KB)':<12} {'Elements':<12} "
          f"{'Naive (ns)':<14} {'Fused (ns)':<14} {'Speedup':<10}")
    print("-" * 100)

    for elements in sorted(naive_data.keys()):
        naive = naive_data[elements]
        fused = fused_data[elements]

        speedup = naive['time_ns'] / fused['time_ns']
        cache_level = categorize_by_cache(naive['size_kb'])

        # Format size
        if naive['size_kb'] >= 1024:
            size_str = f"{naive['size_kb'] / 1024:.0f} MB"
        else:
            size_str = f"{naive['size_kb']:.0f} KB"

        # Format elements
        if elements >= 1024 * 1024:
            elem_str = f"{elements / (1024*1024):.0f}M"
        elif elements >= 1024:
            elem_str = f"{elements / 1024:.0f}K"
        else:
            elem_str = str(elements)

        print(f"{cache_level:<12} {size_str:<12} {elem_str:<12} "
              f"{naive['time_ns']:<14,} {fused['time_ns']:<14,} {speedup:<10.3f}x")

    print("-" * 100)
    print()

    # Summary by cache level
    print("SUMMARY: Speedup by Cache Level")
    print("-" * 100)

    cache_summary = {}
    for elements in sorted(naive_data.keys()):
        naive = naive_data[elements]
        fused = fused_data[elements]
        cache_level = categorize_by_cache(naive['size_kb'])

        speedup = naive['time_ns'] / fused['time_ns']

        if cache_level not in cache_summary:
            cache_summary[cache_level] = []
        cache_summary[cache_level].append(speedup)

    for cache_level in ["L1 (32 KB)", "L2 (1 MB)", "L3 (32 MB)", "DRAM (>L3)"]:
        if cache_level in cache_summary:
            speedups = cache_summary[cache_level]
            avg_speedup = sum(speedups) / len(speedups)
            min_speedup = min(speedups)
            max_speedup = max(speedups)

            print(f"  {cache_level:<15}: {avg_speedup:.3f}x average "
                  f"(range: {min_speedup:.3f}x - {max_speedup:.3f}x)")

    print()
    print("KEY INSIGHTS:")
    print("  1. Fusion advantage is STRONGEST in DRAM (beyond L3 cache)")
    print("  2. When memory-bound, reducing memory ops by 33% (6→4 ops) gives ~1.4x speedup")
    print("  3. In L1/L2 cache, both approaches are fast but fused is still ~1.3x faster")
    print("  4. The naive approach suffers more as data size exceeds cache capacity")
    print()


if __name__ == '__main__':
    main()
