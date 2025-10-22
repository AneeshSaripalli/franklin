#include "container/column.hpp"
#include <benchmark/benchmark.h>
#include <random>

namespace franklin {

// Utility to fill columns with random data
template <typename Policy> void fill_random(column_vector<Policy>& col) {
  std::mt19937 rng(42);

  if constexpr (std::is_same_v<typename Policy::value_type, std::int32_t>) {
    std::uniform_int_distribution<int32_t> dist(-1000, 1000);
    for (size_t i = 0; i < col.data().size(); ++i) {
      col.data()[i] = dist(rng);
    }
  } else if constexpr (std::is_same_v<typename Policy::value_type, float>) {
    std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
    for (size_t i = 0; i < col.data().size(); ++i) {
      col.data()[i] = dist(rng);
    }
  } else if constexpr (std::is_same_v<typename Policy::value_type, bf16>) {
    std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
    for (size_t i = 0; i < col.data().size(); ++i) {
      col.data()[i] = bf16::from_float_trunc(dist(rng));
    }
  }
}

// ============================================================================
// ELEMENT-WISE OPERATIONS - Expected to be MEMORY-BOUND
// ============================================================================

// Int32 element-wise addition: reads 2 columns, writes 1 column = 12
// bytes/element
static void BM_Int32_Add_ElementWise(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Int32DefaultPolicy> a(size);
  column_vector<Int32DefaultPolicy> b(size);
  column_vector<Int32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize bypasses operator overload overhead
    vectorize<Int32DefaultPolicy, Int32Pipeline<OpType::Add>>(a, b, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(int32_t) * 3; // 2 reads + 1 write
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Int32_Mul_ElementWise(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Int32DefaultPolicy> a(size);
  column_vector<Int32DefaultPolicy> b(size);
  column_vector<Int32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize bypasses operator overload overhead
    vectorize<Int32DefaultPolicy, Int32Pipeline<OpType::Mul>>(a, b, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(int32_t) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// Float32 operations
static void BM_Float32_Add_ElementWise(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> b(size);
  column_vector<Float32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize bypasses operator overload overhead
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Add>>(a, b, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(float) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Float32_Mul_ElementWise(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> b(size);
  column_vector<Float32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize bypasses operator overload overhead
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Mul>>(a, b, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(float) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// BF16 operations - interesting because of conversion overhead
static void BM_BF16_Add_ElementWise(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<BF16DefaultPolicy> a(size);
  column_vector<BF16DefaultPolicy> b(size);
  column_vector<BF16DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize bypasses operator overload overhead
    vectorize<BF16DefaultPolicy, BF16Pipeline<OpType::Add>>(a, b, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(bf16) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_BF16_Mul_ElementWise(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<BF16DefaultPolicy> a(size);
  column_vector<BF16DefaultPolicy> b(size);
  column_vector<BF16DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize bypasses operator overload overhead
    vectorize<BF16DefaultPolicy, BF16Pipeline<OpType::Mul>>(a, b, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(bf16) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// ============================================================================
// SCALAR OPERATIONS - Less memory traffic (only 2x instead of 3x)
// ============================================================================

static void BM_Int32_Mul_Scalar(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Int32DefaultPolicy> a(size);
  column_vector<Int32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  const int32_t scalar = 7;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize_scalar bypasses operator overload overhead
    vectorize_scalar<Int32DefaultPolicy, Int32ScalarPipeline<OpType::Mul>>(
        a, scalar, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(int32_t) * 2; // 1 read + 1 write
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Float32_Mul_Scalar(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  const float scalar = 2.5f;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize_scalar bypasses operator overload overhead
    vectorize_scalar<Float32DefaultPolicy, Float32ScalarPipeline<OpType::Mul>>(
        a, scalar, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(float) * 2;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Float32_FMA_Scalar(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> temp(size);   // Pre-allocate temp
  column_vector<Float32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  const float scale = 2.5f;
  const float offset = 10.0f;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // a * scale + offset (potential FMA candidate)
    // Decomposed into two direct vectorize_scalar calls to avoid operator
    // overhead
    vectorize_scalar<Float32DefaultPolicy, Float32ScalarPipeline<OpType::Mul>>(
        a, scale, temp);
    vectorize_scalar<Float32DefaultPolicy, Float32ScalarPipeline<OpType::Add>>(
        temp, offset, result);
    benchmark::DoNotOptimize(result.data().data());
    // This does: read a, write temp, read temp, write result = 4x traffic
    // With fusion: read a, write result = 2x traffic
    bytes_processed += size * sizeof(float) * 4; // Unfused traffic
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// ============================================================================
// COMPLEX EXPRESSIONS - Show fusion opportunities
// ============================================================================

static void BM_Float32_ComplexUnfused(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> b(size);
  column_vector<Float32DefaultPolicy> c(size);
  column_vector<Float32DefaultPolicy> temp(size);   // Pre-allocate temp
  column_vector<Float32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);
  fill_random(c);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // (a + b) * c
    // Unfused: read a, read b, write temp1, read temp1, read c, write result =
    // 6x
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Add>>(a, b, temp);
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Mul>>(temp, c,
                                                                  result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(float) * 6;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
  state.counters["Theoretical_Fused_GB/s"] = benchmark::Counter(
      size * sizeof(float) * 4 * state.iterations(), // Fused would be 4x
      benchmark::Counter::kIsRate, benchmark::Counter::kIs1024);
}

static void BM_Float32_Polynomial(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> x(size);

  fill_random(x);

  // Pre-allocate all intermediate buffers
  column_vector<Float32DefaultPolicy> x2(size);
  column_vector<Float32DefaultPolicy> x3(size);
  column_vector<Float32DefaultPolicy> term1(size);
  column_vector<Float32DefaultPolicy> term2(size);
  column_vector<Float32DefaultPolicy> term3(size);
  column_vector<Float32DefaultPolicy> temp_sum1(size);
  column_vector<Float32DefaultPolicy> temp_sum2(size);
  column_vector<Float32DefaultPolicy> result(size);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Polynomial: x^3 + 2*x^2 + 3*x + 4
    // Without fusion: many intermediate allocations
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Mul>>(x, x, x2);
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Mul>>(x2, x, x3);
    term1 = x3; // Direct assignment (bitset copy only)
    vectorize_scalar<Float32DefaultPolicy, Float32ScalarPipeline<OpType::Mul>>(
        x2, 2.0f, term2);
    vectorize_scalar<Float32DefaultPolicy, Float32ScalarPipeline<OpType::Mul>>(
        x, 3.0f, term3);
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Add>>(term1, term2,
                                                                  temp_sum1);
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Add>>(
        temp_sum1, term3, temp_sum2);
    vectorize_scalar<Float32DefaultPolicy, Float32ScalarPipeline<OpType::Add>>(
        temp_sum2, 4.0f, result);
    benchmark::DoNotOptimize(result.data().data());

    // Actual memory traffic (very rough estimate)
    bytes_processed += size * sizeof(float) * 14; // Many intermediate writes
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
  state.counters["Theoretical_Fused_GB/s"] = benchmark::Counter(
      size * sizeof(float) * 2 *
          state.iterations(), // Fused: read x, write result
      benchmark::Counter::kIsRate, benchmark::Counter::kIs1024);
}

// ============================================================================
// CACHE EFFECTS - Different working set sizes
// ============================================================================

static void BM_Float32_Add_CacheEffects(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> b(size);
  column_vector<Float32DefaultPolicy> result(size); // Pre-allocate output

  fill_random(a);
  fill_random(b);

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Direct call to vectorize bypasses operator overload overhead
    vectorize<Float32DefaultPolicy, Float32Pipeline<OpType::Add>>(a, b, result);
    benchmark::DoNotOptimize(result.data().data());
    bytes_processed += size * sizeof(float) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);

  // Indicate which cache level this fits in
  const size_t total_bytes = size * sizeof(float) * 2;
  if (total_bytes < 32 * 1024) {
    state.SetLabel("L1");
  } else if (total_bytes < 256 * 1024) {
    state.SetLabel("L2");
  } else if (total_bytes < 8 * 1024 * 1024) {
    state.SetLabel("L3");
  } else {
    state.SetLabel("DRAM");
  }
}

// ============================================================================
// REGISTER ARGUMENTS
// ============================================================================

// Small sizes (L1 cache)
BENCHMARK(BM_Int32_Add_ElementWise)->Arg(1024)->Arg(4096);
BENCHMARK(BM_Int32_Mul_ElementWise)->Arg(1024)->Arg(4096);
BENCHMARK(BM_Float32_Add_ElementWise)->Arg(1024)->Arg(4096);
BENCHMARK(BM_Float32_Mul_ElementWise)->Arg(1024)->Arg(4096);
BENCHMARK(BM_BF16_Add_ElementWise)->Arg(1024)->Arg(4096);
BENCHMARK(BM_BF16_Mul_ElementWise)->Arg(1024)->Arg(4096);

// Scalar operations
BENCHMARK(BM_Int32_Mul_Scalar)->Arg(1024)->Arg(4096)->Arg(1024 * 1024);
BENCHMARK(BM_Float32_Mul_Scalar)->Arg(1024)->Arg(4096)->Arg(1024 * 1024);
BENCHMARK(BM_Float32_FMA_Scalar)->Arg(1024)->Arg(4096)->Arg(1024 * 1024);

// Complex expressions showing fusion opportunities
BENCHMARK(BM_Float32_ComplexUnfused)->Arg(4096)->Arg(1024 * 1024);
BENCHMARK(BM_Float32_Polynomial)->Arg(4096)->Arg(1024 * 1024);

// Cache effects - sweep from L1 to DRAM
BENCHMARK(BM_Float32_Add_CacheEffects)
    ->Arg(1024)              // ~4KB (L1)
    ->Arg(8192)              // ~32KB (L1/L2)
    ->Arg(32768)             // ~128KB (L2)
    ->Arg(131072)            // ~512KB (L2/L3)
    ->Arg(1024 * 1024)       // ~4MB (L3)
    ->Arg(4 * 1024 * 1024)   // ~16MB (DRAM)
    ->Arg(16 * 1024 * 1024); // ~64MB (DRAM)

} // namespace franklin

BENCHMARK_MAIN();
