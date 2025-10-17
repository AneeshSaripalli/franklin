#include "container/dynamic_bitset.hpp"
#include <benchmark/benchmark.h>
#include <cstddef>

// Define a policy for testing
struct BenchmarkPolicy {
  using size_type = std::size_t;
};

using Bitset = franklin::dynamic_bitset<BenchmarkPolicy>;

// ============================================================================
// CONSTRUCTION BENCHMARKS
// ============================================================================

static void BM_Construction_Empty(benchmark::State& state) {
  for (auto _ : state) {
    Bitset bs;
    benchmark::DoNotOptimize(bs);
  }
}
BENCHMARK(BM_Construction_Empty);

static void BM_Construction_WithSize_False(benchmark::State& state) {
  const auto size = state.range(0);
  for (auto _ : state) {
    Bitset bs(size, false);
    benchmark::DoNotOptimize(bs);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Construction_WithSize_False)->Range(64, 1 << 20);

static void BM_Construction_WithSize_True(benchmark::State& state) {
  const auto size = state.range(0);
  for (auto _ : state) {
    Bitset bs(size, true);
    benchmark::DoNotOptimize(bs);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Construction_WithSize_True)->Range(64, 1 << 20);

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
BENCHMARK(BM_Set_SingleBit)->Range(64, 1 << 20);

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
BENCHMARK(BM_Test_SingleBit)->Range(64, 1 << 20);

static void BM_Flip_SingleBit(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);
  size_t pos = 0;

  for (auto _ : state) {
    bs.flip(pos);
    pos = (pos + 1) % size;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Flip_SingleBit)->Range(64, 1 << 20);

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
BENCHMARK(BM_SetAll)->Range(64, 1 << 20);

static void BM_ResetAll(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);

  for (auto _ : state) {
    bs.reset();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_ResetAll)->Range(64, 1 << 20);

static void BM_FlipAll(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);

  for (auto _ : state) {
    bs.flip();
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_FlipAll)->Range(64, 1 << 20);

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
BENCHMARK(BM_Count_AllSet)->Range(64, 1 << 20);

static void BM_Count_HalfSet(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);
  // Set every other bit
  for (size_t i = 0; i < size; i += 2) {
    bs.set(i, true);
  }

  for (auto _ : state) {
    auto result = bs.count();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Count_HalfSet)->Range(64, 1 << 20);

static void BM_Any_AllZero(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);

  for (auto _ : state) {
    bool result = bs.any();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Any_AllZero)->Range(64, 1 << 20);

static void BM_Any_OneSet(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);
  bs.set(size / 2, true);

  for (auto _ : state) {
    bool result = bs.any();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_Any_OneSet)->Range(64, 1 << 20);

static void BM_All_AllSet(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);

  for (auto _ : state) {
    bool result = bs.all();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_All_AllSet)->Range(64, 1 << 20);

static void BM_All_OneMissing(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);
  bs.set(size / 2, false);

  for (auto _ : state) {
    bool result = bs.all();
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_All_OneMissing)->Range(64, 1 << 20);

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
BENCHMARK(BM_BitwiseAND)->Range(64, 1 << 20);

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
BENCHMARK(BM_BitwiseOR)->Range(64, 1 << 20);

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
BENCHMARK(BM_BitwiseXOR)->Range(64, 1 << 20);

// ============================================================================
// RESIZE OPERATIONS
// ============================================================================

static void BM_Resize_Grow_False(benchmark::State& state) {
  const auto initial_size = state.range(0);
  const auto final_size = initial_size * 2;

  for (auto _ : state) {
    state.PauseTiming();
    Bitset bs(initial_size, false);
    state.ResumeTiming();

    bs.resize(final_size, false);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * final_size);
}
BENCHMARK(BM_Resize_Grow_False)->Range(64, 1 << 18);

static void BM_Resize_Grow_True(benchmark::State& state) {
  const auto initial_size = state.range(0);
  const auto final_size = initial_size * 2;

  for (auto _ : state) {
    state.PauseTiming();
    Bitset bs(initial_size, false);
    state.ResumeTiming();

    bs.resize(final_size, true);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * final_size);
}
BENCHMARK(BM_Resize_Grow_True)->Range(64, 1 << 18);

static void BM_Resize_Shrink(benchmark::State& state) {
  const auto initial_size = state.range(0) * 2;
  const auto final_size = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    Bitset bs(initial_size, true);
    state.ResumeTiming();

    bs.resize(final_size, false);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * initial_size);
}
BENCHMARK(BM_Resize_Shrink)->Range(64, 1 << 18);

// ============================================================================
// SEQUENTIAL ACCESS PATTERNS
// ============================================================================

static void BM_SequentialWrite(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);

  for (auto _ : state) {
    for (size_t i = 0; i < size; ++i) {
      bs.set(i, true);
    }
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_SequentialWrite)->Range(64, 1 << 20);

static void BM_SequentialRead(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);

  for (auto _ : state) {
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
      count += bs[i];
    }
    benchmark::DoNotOptimize(count);
  }
  state.SetItemsProcessed(state.iterations() * size);
}
BENCHMARK(BM_SequentialRead)->Range(64, 1 << 20);

// ============================================================================
// RANDOM ACCESS PATTERNS
// ============================================================================

static void BM_RandomWrite(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, false);

  // Pre-generate random indices to avoid measuring RNG overhead
  std::vector<size_t> indices(10000);
  for (size_t i = 0; i < indices.size(); ++i) {
    indices[i] = (i * 7919) % size; // Simple pseudo-random
  }

  size_t idx = 0;
  for (auto _ : state) {
    bs.set(indices[idx % indices.size()], true);
    ++idx;
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RandomWrite)->Range(64, 1 << 20);

static void BM_RandomRead(benchmark::State& state) {
  const auto size = state.range(0);
  Bitset bs(size, true);

  // Pre-generate random indices
  std::vector<size_t> indices(10000);
  for (size_t i = 0; i < indices.size(); ++i) {
    indices[i] = (i * 7919) % size;
  }

  size_t idx = 0;
  for (auto _ : state) {
    bool result = bs[indices[idx % indices.size()]];
    benchmark::DoNotOptimize(result);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RandomRead)->Range(64, 1 << 20);

BENCHMARK_MAIN();
