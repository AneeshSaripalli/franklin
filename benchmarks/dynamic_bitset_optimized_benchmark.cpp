#include "container/dynamic_bitset_optimized.hpp"
#include <benchmark/benchmark.h>
#include <cstddef>

// Define a policy for testing
struct BenchmarkPolicy {
  using size_type = std::size_t;
};

using Bitset = franklin::dynamic_bitset<BenchmarkPolicy>;

// ============================================================================
// SINGLE BIT OPERATIONS
// ============================================================================

static void BM_Set_SingleBit(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);
  size_t pos = 0;

  for (auto _ : state) {
    bs.set(pos, true);
    pos = (pos + 1) % size;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Set_SingleBit)->Arg(1048576);

static void BM_Test_SingleBit(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);
  size_t pos = 0;

  for (auto _ : state) {
    bool result = bs[pos];
    benchmark::DoNotOptimize(result);
    pos = (pos + 1) % size;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Test_SingleBit)->Arg(1048576);

// ============================================================================
// BULK OPERATIONS
// ============================================================================

static void BM_SetAll(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);

  for (auto _ : state) {
    bs.set();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_SetAll)->Arg(1048576);

static void BM_ResetAll(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);

  for (auto _ : state) {
    bs.reset();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_ResetAll)->Arg(1048576);

static void BM_FlipAll(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);

  for (auto _ : state) {
    bs.flip();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_FlipAll)->Arg(1048576);

// ============================================================================
// QUERY OPERATIONS
// ============================================================================

static void BM_Count_AllSet(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);

  for (auto _ : state) {
    auto result = bs.count();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Count_AllSet)->Arg(1048576);

static void BM_Any_AllZero(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);

  for (auto _ : state) {
    bool result = bs.any();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Any_AllZero)->Arg(1048576);

static void BM_All_AllSet(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);

  for (auto _ : state) {
    bool result = bs.all();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_All_AllSet)->Arg(1048576);

// ============================================================================
// BITWISE OPERATIONS
// ============================================================================

static void BM_BitwiseAND(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs1(size, true);
  Bitset bs2(size, true);

  for (auto _ : state) {
    bs1 &= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_BitwiseAND)->Arg(1048576);

static void BM_BitwiseOR(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs1(size, false);
  Bitset bs2(size, true);

  for (auto _ : state) {
    bs1 |= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_BitwiseOR)->Arg(1048576);

static void BM_BitwiseXOR(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs1(size, true);
  Bitset bs2(size, false);

  for (auto _ : state) {
    bs1 ^= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_BitwiseXOR)->Arg(1048576);

BENCHMARK_MAIN();
