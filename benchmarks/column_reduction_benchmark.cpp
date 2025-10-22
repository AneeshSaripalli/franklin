#include "container/column.hpp"
#include "core/bf16.hpp"
#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>

namespace franklin {

// ============================================================================
// Int32 Reduction Benchmarks
// ============================================================================

static void BM_Int32Sum_1K(benchmark::State& state) {
  const size_t size = 1024;
  column_vector<Int32DefaultPolicy> col(size);

  // Fill with random data
  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(1, 100);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    int32_t result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  // Report throughput: bytes processed / time
  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Sum_1K);

static void BM_Int32Sum_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(1, 100);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    int32_t result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Sum_64K);

static void BM_Int32Sum_1M(benchmark::State& state) {
  const size_t size = 1024 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(1, 100);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    int32_t result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Sum_1M);

static void BM_Int32Product_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(1, 3);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    int32_t result = col.product();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Product_64K);

static void BM_Int32Min_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(0, 10000);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    int32_t result = col.min();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Min_64K);

static void BM_Int32Max_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(0, 10000);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    int32_t result = col.max();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Max_64K);

// ============================================================================
// Int32 with Sparse Present Mask
// ============================================================================

static void BM_Int32Sum_64K_Sparse50(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(1, 100);
  std::bernoulli_distribution present_dist(0.5); // 50% present

  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
    col.present_mask().set(i, present_dist(rng));
  }

  for (auto _ : state) {
    int32_t result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Sum_64K_Sparse50);

static void BM_Int32Sum_64K_Sparse10(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_int_distribution<int32_t> dist(1, 100);
  std::bernoulli_distribution present_dist(0.1); // 10% present

  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
    col.present_mask().set(i, present_dist(rng));
  }

  for (auto _ : state) {
    int32_t result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(int32_t);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Int32Sum_64K_Sparse10);

// ============================================================================
// Float32 Reduction Benchmarks
// ============================================================================

static void BM_Float32Sum_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Float32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(1.0f, 100.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    float result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(float);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Float32Sum_64K);

static void BM_Float32Sum_1M(benchmark::State& state) {
  const size_t size = 1024 * 1024;
  column_vector<Float32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(1.0f, 100.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    float result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(float);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Float32Sum_1M);

static void BM_Float32Product_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Float32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.95f, 1.05f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    float result = col.product();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(float);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Float32Product_64K);

static void BM_Float32Min_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Float32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.0f, 10000.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    float result = col.min();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(float);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Float32Min_64K);

static void BM_Float32Max_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Float32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.0f, 10000.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = dist(rng);
  }

  for (auto _ : state) {
    float result = col.max();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(float);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_Float32Max_64K);

// ============================================================================
// BF16 Reduction Benchmarks
// ============================================================================

static void BM_BF16Sum_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<BF16DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(1.0f, 100.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = bf16::from_float_trunc(dist(rng));
  }

  for (auto _ : state) {
    bf16 result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(bf16);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_BF16Sum_64K);

static void BM_BF16Sum_1M(benchmark::State& state) {
  const size_t size = 1024 * 1024;
  column_vector<BF16DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(1.0f, 100.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = bf16::from_float_trunc(dist(rng));
  }

  for (auto _ : state) {
    bf16 result = col.sum();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(bf16);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_BF16Sum_1M);

static void BM_BF16Product_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<BF16DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.95f, 1.05f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = bf16::from_float_trunc(dist(rng));
  }

  for (auto _ : state) {
    bf16 result = col.product();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(bf16);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_BF16Product_64K);

static void BM_BF16Min_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<BF16DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.0f, 10000.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = bf16::from_float_trunc(dist(rng));
  }

  for (auto _ : state) {
    bf16 result = col.min();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(bf16);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_BF16Min_64K);

static void BM_BF16Max_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<BF16DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(0.0f, 10000.0f);
  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = bf16::from_float_trunc(dist(rng));
  }

  for (auto _ : state) {
    bf16 result = col.max();
    benchmark::DoNotOptimize(result);
  }

  const size_t bytes_per_iteration = size * sizeof(bf16);
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration);
}
BENCHMARK(BM_BF16Max_64K);

// ============================================================================
// Any/All Benchmarks
// ============================================================================

static void BM_AnyAll_Dense_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = static_cast<int32_t>(i);
  }

  for (auto _ : state) {
    bool any_result = col.any();
    bool all_result = col.all();
    benchmark::DoNotOptimize(any_result);
    benchmark::DoNotOptimize(all_result);
  }

  // Bytes processed for bitmask operations (just the bitmask, not the data)
  const size_t bytes_per_iteration = (size + 7) / 8;
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration *
                          2); // 2 ops
}
BENCHMARK(BM_AnyAll_Dense_64K);

static void BM_AnyAll_Sparse_64K(benchmark::State& state) {
  const size_t size = 64 * 1024;
  column_vector<Int32DefaultPolicy> col(size);

  std::mt19937 rng(42);
  std::bernoulli_distribution present_dist(0.5);

  for (size_t i = 0; i < size; ++i) {
    col.data()[i] = static_cast<int32_t>(i);
    col.present_mask().set(i, present_dist(rng));
  }

  for (auto _ : state) {
    bool any_result = col.any();
    bool all_result = col.all();
    benchmark::DoNotOptimize(any_result);
    benchmark::DoNotOptimize(all_result);
  }

  const size_t bytes_per_iteration = (size + 7) / 8;
  state.SetBytesProcessed(state.iterations() * bytes_per_iteration * 2);
}
BENCHMARK(BM_AnyAll_Sparse_64K);

} // namespace franklin

BENCHMARK_MAIN();
