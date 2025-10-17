// Benchmark to evaluate the value of kernel loop fusion
//
// This benchmark compares:
// 1. Naive approach: tmp = a * b (loop 1), then tmp += c (loop 2)
// 2. Fused approach: result = a * b + c (single loop with FMA)
//
// Columns are sized larger than L3 cache to ensure memory-bound performance.
// The naive approach loads data twice and writes to memory twice.
// The fused approach loads data once and writes once, demonstrating the
// value of avoiding temporaries and reducing memory traffic.

#include "container/column.hpp"
#include <benchmark/benchmark.h>
#include <immintrin.h>
#include <vector>

namespace franklin {
namespace {

using Float32Policy = Float32DefaultPolicy;
using Float32Column = column_vector<Float32Policy>;

// Typical L3 cache sizes: 8-32 MB
// Use 128 MB per column (32M floats * 4 bytes = 128 MB)
// This ensures we're memory-bound, not cache-bound
constexpr size_t COLUMN_SIZE = 32 * 1024 * 1024; // 32M elements = 128 MB

// ============================================================================
// NAIVE IMPLEMENTATION: Two separate loops in separate functions
// ============================================================================

// Multiply a * b, store in result
// __attribute__((noinline)) prevents compiler from inlining and fusing loops
__attribute__((noinline)) void naive_multiply(const float* __restrict__ a,
                                              const float* __restrict__ b,
                                              float* __restrict__ result,
                                              size_t n) {
  // Process 8 floats at a time with AVX2
  const size_t simd_end = (n / 8) * 8;

  for (size_t i = 0; i < simd_end; i += 8) {
    __m256 va = _mm256_load_ps(a + i);
    __m256 vb = _mm256_load_ps(b + i);
    __m256 vr = _mm256_mul_ps(va, vb);
    _mm256_store_ps(result + i, vr);
  }

  // Handle remaining elements
  for (size_t i = simd_end; i < n; ++i) {
    result[i] = a[i] * b[i];
  }
}

// Add c to result (result += c)
// __attribute__((noinline)) prevents compiler from inlining and fusing loops
__attribute__((noinline)) void naive_add(const float* __restrict__ c,
                                         float* __restrict__ result, size_t n) {
  // Process 8 floats at a time with AVX2
  const size_t simd_end = (n / 8) * 8;

  for (size_t i = 0; i < simd_end; i += 8) {
    __m256 vr = _mm256_load_ps(result + i);
    __m256 vc = _mm256_load_ps(c + i);
    vr = _mm256_add_ps(vr, vc);
    _mm256_store_ps(result + i, vr);
  }

  // Handle remaining elements
  for (size_t i = simd_end; i < n; ++i) {
    result[i] += c[i];
  }
}

// Naive version: two separate loops, two memory passes
void naive_fma_two_loops(const Float32Column& a, const Float32Column& b,
                         const Float32Column& c, Float32Column& result) {
  const size_t n = a.data().size();
  const float* a_ptr = a.data().data();
  const float* b_ptr = b.data().data();
  const float* c_ptr = c.data().data();
  float* result_ptr = result.data().data();

  // Loop 1: tmp = a * b
  naive_multiply(a_ptr, b_ptr, result_ptr, n);

  // Loop 2: tmp += c
  naive_add(c_ptr, result_ptr, n);
}

// ============================================================================
// FUSED IMPLEMENTATION: Single loop with FMA instruction
// ============================================================================

// Fused version: single loop using FMA, single memory pass
__attribute__((noinline)) void fused_fma_single_loop(const Float32Column& a,
                                                     const Float32Column& b,
                                                     const Float32Column& c,
                                                     Float32Column& result) {
  const size_t n = a.data().size();
  const float* __restrict__ a_ptr = a.data().data();
  const float* __restrict__ b_ptr = b.data().data();
  const float* __restrict__ c_ptr = c.data().data();
  float* __restrict__ result_ptr = result.data().data();

  // Process 8 floats at a time with AVX2 FMA
  const size_t simd_end = (n / 8) * 8;

  for (size_t i = 0; i < simd_end; i += 8) {
    __m256 va = _mm256_load_ps(a_ptr + i);
    __m256 vb = _mm256_load_ps(b_ptr + i);
    __m256 vc = _mm256_load_ps(c_ptr + i);
    // FMA: result = a * b + c (single instruction, single memory write)
    __m256 vr = _mm256_fmadd_ps(va, vb, vc);
    _mm256_store_ps(result_ptr + i, vr);
  }

  // Handle remaining elements
  for (size_t i = simd_end; i < n; ++i) {
    result_ptr[i] = a_ptr[i] * b_ptr[i] + c_ptr[i];
  }
}

// ============================================================================
// BENCHMARK FIXTURES
// ============================================================================

class KernelFusionFixture : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State& state) override {
    // Initialize columns with data larger than L3 cache
    a_ = Float32Column(COLUMN_SIZE, 2.0f);
    b_ = Float32Column(COLUMN_SIZE, 3.0f);
    c_ = Float32Column(COLUMN_SIZE, 5.0f);
    result_ = Float32Column(COLUMN_SIZE, 0.0f);

    // Verify data is initialized
    benchmark::DoNotOptimize(a_.data().data());
    benchmark::DoNotOptimize(b_.data().data());
    benchmark::DoNotOptimize(c_.data().data());
  }

protected:
  Float32Column a_;
  Float32Column b_;
  Float32Column c_;
  Float32Column result_;
};

// ============================================================================
// BENCHMARKS
// ============================================================================

BENCHMARK_F(KernelFusionFixture, Naive_TwoLoops_MulThenAdd)
(benchmark::State& state) {
  for (auto _ : state) {
    naive_fma_two_loops(a_, b_, c_, result_);
    // Prevent compiler from optimizing away the result
    benchmark::DoNotOptimize(result_.data().data());
    benchmark::ClobberMemory();
  }

  // Verify correctness: 2.0 * 3.0 + 5.0 = 11.0
  if (result_.data()[0] != 11.0f) {
    state.SkipWithError("Incorrect result in Naive_TwoLoops");
  }

  // Report memory bandwidth: 4 loads + 2 stores per iteration
  const size_t bytes_per_iter = COLUMN_SIZE * sizeof(float) * 6;
  state.SetBytesProcessed(state.iterations() * bytes_per_iter);
  state.counters["elements"] = benchmark::Counter(COLUMN_SIZE);
  state.counters["loads_per_element"] = benchmark::Counter(4.0);
  state.counters["stores_per_element"] = benchmark::Counter(2.0);
}

BENCHMARK_F(KernelFusionFixture, Fused_SingleLoop_FMA)
(benchmark::State& state) {
  for (auto _ : state) {
    fused_fma_single_loop(a_, b_, c_, result_);
    // Prevent compiler from optimizing away the result
    benchmark::DoNotOptimize(result_.data().data());
    benchmark::ClobberMemory();
  }

  // Verify correctness: 2.0 * 3.0 + 5.0 = 11.0
  if (result_.data()[0] != 11.0f) {
    state.SkipWithError("Incorrect result in Fused_SingleLoop");
  }

  // Report memory bandwidth: 3 loads + 1 store per iteration
  const size_t bytes_per_iter = COLUMN_SIZE * sizeof(float) * 4;
  state.SetBytesProcessed(state.iterations() * bytes_per_iter);
  state.counters["elements"] = benchmark::Counter(COLUMN_SIZE);
  state.counters["loads_per_element"] = benchmark::Counter(3.0);
  state.counters["stores_per_element"] = benchmark::Counter(1.0);
}

// ============================================================================
// ANALYSIS BENCHMARK: Measure memory traffic difference
// ============================================================================

BENCHMARK_F(KernelFusionFixture, Analysis_MemoryTraffic_Naive)
(benchmark::State& state) {
  for (auto _ : state) {
    naive_fma_two_loops(a_, b_, c_, result_);
    benchmark::DoNotOptimize(result_.data().data());
    benchmark::ClobberMemory();
  }

  // Naive approach memory traffic:
  // Loop 1 (multiply): Load a, load b, store result = 3 ops
  // Loop 2 (add): Load result, load c, store result = 3 ops
  // Total: 4 loads + 2 stores per element
  state.counters["loads_per_iter"] = benchmark::Counter(COLUMN_SIZE * 4);
  state.counters["stores_per_iter"] = benchmark::Counter(COLUMN_SIZE * 2);
  state.counters["total_memory_ops"] = benchmark::Counter(COLUMN_SIZE * 6);
  state.counters["memory_traffic_reduction"] =
      benchmark::Counter(0.0); // Baseline
}

BENCHMARK_F(KernelFusionFixture, Analysis_MemoryTraffic_Fused)
(benchmark::State& state) {
  for (auto _ : state) {
    fused_fma_single_loop(a_, b_, c_, result_);
    benchmark::DoNotOptimize(result_.data().data());
    benchmark::ClobberMemory();
  }

  // Fused approach memory traffic:
  // Single loop: Load a, load b, load c, store result = 4 ops
  // Total: 3 loads + 1 store per element
  state.counters["loads_per_iter"] = benchmark::Counter(COLUMN_SIZE * 3);
  state.counters["stores_per_iter"] = benchmark::Counter(COLUMN_SIZE * 1);
  state.counters["total_memory_ops"] = benchmark::Counter(COLUMN_SIZE * 4);
  // 33% reduction: (6 - 4) / 6 = 0.33
  state.counters["memory_traffic_reduction"] = benchmark::Counter(0.33);
}

} // namespace
} // namespace franklin

BENCHMARK_MAIN();
