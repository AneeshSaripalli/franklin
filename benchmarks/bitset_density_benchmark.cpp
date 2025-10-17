#include "container/dynamic_bitset.hpp"
#include <benchmark/benchmark.h>
#include <cstddef>
#include <random>

// Define a policy for testing
struct BenchmarkPolicy {
  using size_type = std::size_t;
};

using Bitset = franklin::dynamic_bitset<BenchmarkPolicy>;

// Helper to create bitset with specific density of set bits, uniformly distributed
Bitset create_bitset_with_density(size_t size, double density_percent) {
  Bitset bs(size, false);

  if (density_percent >= 100.0) {
    bs.set();  // Set all bits
    return bs;
  }

  if (density_percent <= 0.0) {
    return bs;  // All zeros
  }

  // Calculate number of bits to set
  size_t num_to_set = static_cast<size_t>(size * (density_percent / 100.0));

  // Use deterministic random for reproducibility
  std::mt19937_64 rng(42);
  std::uniform_int_distribution<size_t> dist(0, size - 1);

  // Set bits randomly until we reach target density
  size_t set_count = 0;
  while (set_count < num_to_set) {
    size_t pos = dist(rng);
    if (!bs[pos]) {
      bs.set(pos, true);
      set_count++;
    }
  }

  return bs;
}

// ============================================================================
// ANY() BENCHMARKS - Various densities
// ============================================================================

static void BM_Any_Density(benchmark::State& state) {
  const auto size = state.range(0);
  const auto density = state.range(1);

  auto bs = create_bitset_with_density(size, density);

  for (auto _ : state) {
    bool result = bs.any();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
  state.SetBytesProcessed(state.iterations() * (size / 8));
  state.counters["density"] = density;
}

BENCHMARK(BM_Any_Density)
    ->Args({1024, 0})
    ->Args({1024, 1})
    ->Args({1024, 5})
    ->Args({1024, 10})
    ->Args({1024, 20})
    ->Args({1024, 50})
    ->Args({1024, 80})
    ->Args({1024, 100})
    ->Args({65536, 0})
    ->Args({65536, 1})
    ->Args({65536, 5})
    ->Args({65536, 10})
    ->Args({65536, 20})
    ->Args({65536, 50})
    ->Args({65536, 80})
    ->Args({65536, 100})
    ->Args({1048576, 0})
    ->Args({1048576, 1})
    ->Args({1048576, 5})
    ->Args({1048576, 10})
    ->Args({1048576, 20})
    ->Args({1048576, 50})
    ->Args({1048576, 80})
    ->Args({1048576, 100});

// ============================================================================
// ALL() BENCHMARKS - Various densities
// ============================================================================

static void BM_All_Density(benchmark::State& state) {
  const auto size = state.range(0);
  const auto density = state.range(1);

  auto bs = create_bitset_with_density(size, density);

  for (auto _ : state) {
    bool result = bs.all();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
  state.SetBytesProcessed(state.iterations() * (size / 8));
  state.counters["density"] = density;
}

BENCHMARK(BM_All_Density)
    ->Args({1024, 0})
    ->Args({1024, 1})
    ->Args({1024, 5})
    ->Args({1024, 10})
    ->Args({1024, 20})
    ->Args({1024, 50})
    ->Args({1024, 80})
    ->Args({1024, 100})
    ->Args({65536, 0})
    ->Args({65536, 1})
    ->Args({65536, 5})
    ->Args({65536, 10})
    ->Args({65536, 20})
    ->Args({65536, 50})
    ->Args({65536, 80})
    ->Args({65536, 100})
    ->Args({1048576, 0})
    ->Args({1048576, 1})
    ->Args({1048576, 5})
    ->Args({1048576, 10})
    ->Args({1048576, 20})
    ->Args({1048576, 50})
    ->Args({1048576, 80})
    ->Args({1048576, 100});

// ============================================================================
// NONE() BENCHMARKS - Various densities
// ============================================================================

static void BM_None_Density(benchmark::State& state) {
  const auto size = state.range(0);
  const auto density = state.range(1);

  auto bs = create_bitset_with_density(size, density);

  for (auto _ : state) {
    bool result = bs.none();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
  state.SetBytesProcessed(state.iterations() * (size / 8));
  state.counters["density"] = density;
}

BENCHMARK(BM_None_Density)
    ->Args({1024, 0})
    ->Args({1024, 1})
    ->Args({1024, 5})
    ->Args({1024, 10})
    ->Args({1024, 20})
    ->Args({1024, 50})
    ->Args({1024, 80})
    ->Args({1024, 100})
    ->Args({65536, 0})
    ->Args({65536, 1})
    ->Args({65536, 5})
    ->Args({65536, 10})
    ->Args({65536, 20})
    ->Args({65536, 50})
    ->Args({65536, 80})
    ->Args({65536, 100})
    ->Args({1048576, 0})
    ->Args({1048576, 1})
    ->Args({1048576, 5})
    ->Args({1048576, 10})
    ->Args({1048576, 20})
    ->Args({1048576, 50})
    ->Args({1048576, 80})
    ->Args({1048576, 100});

BENCHMARK_MAIN();
