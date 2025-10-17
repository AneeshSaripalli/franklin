#include <benchmark/benchmark.h>

// Include original version in a namespace
namespace original {
#include "container/dynamic_bitset.hpp"
}

// Include optimized version in a namespace
namespace optimized {
#include "container/dynamic_bitset_optimized.hpp"
}

#include <cstddef>

struct BenchmarkPolicy {
  using size_type = std::size_t;
};

using BitsetOriginal = original::franklin::dynamic_bitset<BenchmarkPolicy>;
using BitsetOptimized = optimized::franklin::dynamic_bitset<BenchmarkPolicy>;

// ============================================================================
// SINGLE BIT OPERATIONS - ORIGINAL
// ============================================================================

static void BM_Original_Set_SingleBit(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs(size, false);
  size_t pos = 0;

  for (auto _ : state) {
    bs.set(pos, true);
    pos = (pos + 1) % size;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Original_Set_SingleBit)->Arg(1048576);

static void BM_Original_Test_SingleBit(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs(size, true);
  size_t pos = 0;

  for (auto _ : state) {
    bool result = bs[pos];
    benchmark::DoNotOptimize(result);
    pos = (pos + 1) % size;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Original_Test_SingleBit)->Arg(1048576);

static void BM_Original_Count(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs(size, true);

  for (auto _ : state) {
    auto result = bs.count();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Original_Count)->Arg(1048576);

static void BM_Original_Any_AllZero(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs(size, false);

  for (auto _ : state) {
    bool result = bs.any();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Original_Any_AllZero)->Arg(1048576);

static void BM_Original_All_AllSet(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs(size, true);

  for (auto _ : state) {
    bool result = bs.all();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Original_All_AllSet)->Arg(1048576);

static void BM_Original_ResetAll(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs(size, true);

  for (auto _ : state) {
    bs.reset();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Original_ResetAll)->Arg(1048576);

static void BM_Original_BitwiseAND(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs1(size, true);
  BitsetOriginal bs2(size, true);

  for (auto _ : state) {
    bs1 &= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Original_BitwiseAND)->Arg(1048576);

static void BM_Original_BitwiseOR(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs1(size, false);
  BitsetOriginal bs2(size, true);

  for (auto _ : state) {
    bs1 |= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Original_BitwiseOR)->Arg(1048576);

static void BM_Original_BitwiseXOR(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOriginal bs1(size, true);
  BitsetOriginal bs2(size, false);

  for (auto _ : state) {
    bs1 ^= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Original_BitwiseXOR)->Arg(1048576);

// ============================================================================
// SINGLE BIT OPERATIONS - OPTIMIZED
// ============================================================================

static void BM_Optimized_Set_SingleBit(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs(size, false);
  size_t pos = 0;

  for (auto _ : state) {
    bs.set(pos, true);
    pos = (pos + 1) % size;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Optimized_Set_SingleBit)->Arg(1048576);

static void BM_Optimized_Test_SingleBit(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs(size, true);
  size_t pos = 0;

  for (auto _ : state) {
    bool result = bs[pos];
    benchmark::DoNotOptimize(result);
    pos = (pos + 1) % size;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Optimized_Test_SingleBit)->Arg(1048576);

static void BM_Optimized_Count(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs(size, true);

  for (auto _ : state) {
    auto result = bs.count();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Optimized_Count)->Arg(1048576);

static void BM_Optimized_Any_AllZero(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs(size, false);

  for (auto _ : state) {
    bool result = bs.any();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Optimized_Any_AllZero)->Arg(1048576);

static void BM_Optimized_All_AllSet(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs(size, true);

  for (auto _ : state) {
    bool result = bs.all();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Optimized_All_AllSet)->Arg(1048576);

static void BM_Optimized_ResetAll(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs(size, true);

  for (auto _ : state) {
    bs.reset();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Optimized_ResetAll)->Arg(1048576);

static void BM_Optimized_BitwiseAND(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs1(size, true);
  BitsetOptimized bs2(size, true);

  for (auto _ : state) {
    bs1 &= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Optimized_BitwiseAND)->Arg(1048576);

static void BM_Optimized_BitwiseOR(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs1(size, false);
  BitsetOptimized bs2(size, true);

  for (auto _ : state) {
    bs1 |= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Optimized_BitwiseOR)->Arg(1048576);

static void BM_Optimized_BitwiseXOR(benchmark::State& state) {
  const auto size = state.range(0);
  BitsetOptimized bs1(size, true);
  BitsetOptimized bs2(size, false);

  for (auto _ : state) {
    bs1 ^= bs2;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Optimized_BitwiseXOR)->Arg(1048576);

BENCHMARK_MAIN();
