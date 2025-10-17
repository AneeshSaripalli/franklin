#include "container/dynamic_bitset.hpp"
#include <benchmark/benchmark.h>

namespace franklin {

struct BitsetPolicy {
  using size_type = std::size_t;
};

// Benchmark bitset AND operation
static void BM_Bitset_AND(benchmark::State& state) {
  const size_t num_bits = state.range(0);
  dynamic_bitset<BitsetPolicy> a(num_bits, true);
  dynamic_bitset<BitsetPolicy> b(num_bits, true);

  // Set some bits to false to make it non-trivial
  for (size_t i = 0; i < num_bits; i += 17) {
    a.set(i, false);
  }
  for (size_t i = 0; i < num_bits; i += 23) {
    b.set(i, false);
  }

  for (auto _ : state) {
    dynamic_bitset<BitsetPolicy> result = a;
    result &= b;
    benchmark::DoNotOptimize(result);
  }

  // Report bits processed per second
  state.counters["Mbits/s"] = benchmark::Counter(num_bits * state.iterations(),
                                                 benchmark::Counter::kIsRate,
                                                 benchmark::Counter::kIs1000);
}

// Benchmark bitset OR operation
static void BM_Bitset_OR(benchmark::State& state) {
  const size_t num_bits = state.range(0);
  dynamic_bitset<BitsetPolicy> a(num_bits, false);
  dynamic_bitset<BitsetPolicy> b(num_bits, false);

  for (size_t i = 0; i < num_bits; i += 17) {
    a.set(i, true);
  }
  for (size_t i = 0; i < num_bits; i += 23) {
    b.set(i, true);
  }

  for (auto _ : state) {
    dynamic_bitset<BitsetPolicy> result = a;
    result |= b;
    benchmark::DoNotOptimize(result);
  }

  state.counters["Mbits/s"] = benchmark::Counter(num_bits * state.iterations(),
                                                 benchmark::Counter::kIsRate,
                                                 benchmark::Counter::kIs1000);
}

// Benchmark bitset XOR operation
static void BM_Bitset_XOR(benchmark::State& state) {
  const size_t num_bits = state.range(0);
  dynamic_bitset<BitsetPolicy> a(num_bits, true);
  dynamic_bitset<BitsetPolicy> b(num_bits, false);

  for (size_t i = 0; i < num_bits; i += 17) {
    a.set(i, false);
  }
  for (size_t i = 0; i < num_bits; i += 23) {
    b.set(i, true);
  }

  for (auto _ : state) {
    dynamic_bitset<BitsetPolicy> result = a;
    result ^= b;
    benchmark::DoNotOptimize(result);
  }

  state.counters["Mbits/s"] = benchmark::Counter(num_bits * state.iterations(),
                                                 benchmark::Counter::kIsRate,
                                                 benchmark::Counter::kIs1000);
}

// Test various sizes from L1 to DRAM
BENCHMARK(BM_Bitset_AND)
    ->Arg(1024)       // 128 bytes (L1)
    ->Arg(8192)       // 1KB (L1)
    ->Arg(65536)      // 8KB (L2)
    ->Arg(524288)     // 64KB (L2)
    ->Arg(4194304)    // 512KB (L3)
    ->Arg(33554432)   // 4MB (L3)
    ->Arg(268435456); // 32MB (DRAM)

BENCHMARK(BM_Bitset_OR)
    ->Arg(1024)
    ->Arg(8192)
    ->Arg(65536)
    ->Arg(524288)
    ->Arg(4194304)
    ->Arg(33554432)
    ->Arg(268435456);

BENCHMARK(BM_Bitset_XOR)
    ->Arg(1024)
    ->Arg(8192)
    ->Arg(65536)
    ->Arg(524288)
    ->Arg(4194304)
    ->Arg(33554432)
    ->Arg(268435456);

} // namespace franklin

BENCHMARK_MAIN();
