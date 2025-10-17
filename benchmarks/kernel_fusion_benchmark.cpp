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

// Cache sizes (typical):
// L1: 32 KB per core
// L2: 256 KB - 1 MB per core
// L3: 8-32 MB shared
//
// Test sizes (elements of float32):
// 8K   =  32 KB  = fits in L1
// 16K  =  64 KB  = exceeds L1
// 32K  = 128 KB  = fits in L2
// 64K  = 256 KB  = fits in L2
// 128K = 512 KB  = fits in L2
// 256K =   1 MB  = fits in L2/L3
// 512K =   2 MB  = fits in L3
// 1M   =   4 MB  = fits in L3
// 2M   =   8 MB  = fits in L3
// 4M   =  16 MB  = fits in L3
// 8M   =  32 MB  = at L3 boundary
// 16M  =  64 MB  = exceeds L3
// 32M  = 128 MB  = exceeds L3
// 64M  = 256 MB  = exceeds L3

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
    // Get array size from benchmark argument
    size_t array_size = state.range(0);

    // Initialize columns with specified size
    a_ = Float32Column(array_size, 2.0f);
    b_ = Float32Column(array_size, 3.0f);
    c_ = Float32Column(array_size, 5.0f);
    result_ = Float32Column(array_size, 0.0f);

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

BENCHMARK_DEFINE_F(KernelFusionFixture, Naive_TwoLoops_MulThenAdd)
(benchmark::State& state) {
  const size_t column_size = state.range(0);

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

  // Report bandwidth: 3 input arrays + 1 output array = 4 arrays * 4 bytes = 16
  // bytes/element
  const size_t bytes_per_iter = column_size * sizeof(float) * 4;
  state.SetBytesProcessed(state.iterations() * bytes_per_iter);
  state.counters["elements"] = benchmark::Counter(column_size);
  state.counters["size_kb"] =
      benchmark::Counter(column_size * sizeof(float) / 1024.0);
}

BENCHMARK_DEFINE_F(KernelFusionFixture, Fused_SingleLoop_FMA)
(benchmark::State& state) {
  const size_t column_size = state.range(0);

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

  // Report bandwidth: 3 input arrays + 1 output array = 4 arrays * 4 bytes = 16
  // bytes/element
  const size_t bytes_per_iter = column_size * sizeof(float) * 4;
  state.SetBytesProcessed(state.iterations() * bytes_per_iter);
  state.counters["elements"] = benchmark::Counter(column_size);
  state.counters["size_kb"] =
      benchmark::Counter(column_size * sizeof(float) / 1024.0);
}

// Register benchmarks with size parameters
// Test sizes from L1 cache (8K elements = 32KB) to beyond L3 (64M elements =
// 256MB)
BENCHMARK_REGISTER_F(KernelFusionFixture, Naive_TwoLoops_MulThenAdd)
    ->Arg(8 * 1024)          // 8K elements = 32 KB (L1)
    ->Arg(16 * 1024)         // 16K elements = 64 KB
    ->Arg(32 * 1024)         // 32K elements = 128 KB (L2)
    ->Arg(64 * 1024)         // 64K elements = 256 KB (L2)
    ->Arg(128 * 1024)        // 128K elements = 512 KB (L2)
    ->Arg(256 * 1024)        // 256K elements = 1 MB (L2/L3)
    ->Arg(512 * 1024)        // 512K elements = 2 MB (L3)
    ->Arg(1024 * 1024)       // 1M elements = 4 MB (L3)
    ->Arg(2 * 1024 * 1024)   // 2M elements = 8 MB (L3)
    ->Arg(4 * 1024 * 1024)   // 4M elements = 16 MB (L3)
    ->Arg(8 * 1024 * 1024)   // 8M elements = 32 MB (L3 boundary)
    ->Arg(16 * 1024 * 1024)  // 16M elements = 64 MB (exceeds L3)
    ->Arg(32 * 1024 * 1024)  // 32M elements = 128 MB (exceeds L3)
    ->Arg(64 * 1024 * 1024); // 64M elements = 256 MB (exceeds L3)

BENCHMARK_REGISTER_F(KernelFusionFixture, Fused_SingleLoop_FMA)
    ->Arg(8 * 1024)          // 8K elements = 32 KB (L1)
    ->Arg(16 * 1024)         // 16K elements = 64 KB
    ->Arg(32 * 1024)         // 32K elements = 128 KB (L2)
    ->Arg(64 * 1024)         // 64K elements = 256 KB (L2)
    ->Arg(128 * 1024)        // 128K elements = 512 KB (L2)
    ->Arg(256 * 1024)        // 256K elements = 1 MB (L2/L3)
    ->Arg(512 * 1024)        // 512K elements = 2 MB (L3)
    ->Arg(1024 * 1024)       // 1M elements = 4 MB (L3)
    ->Arg(2 * 1024 * 1024)   // 2M elements = 8 MB (L3)
    ->Arg(4 * 1024 * 1024)   // 4M elements = 16 MB (L3)
    ->Arg(8 * 1024 * 1024)   // 8M elements = 32 MB (L3 boundary)
    ->Arg(16 * 1024 * 1024)  // 16M elements = 64 MB (exceeds L3)
    ->Arg(32 * 1024 * 1024)  // 32M elements = 128 MB (exceeds L3)
    ->Arg(64 * 1024 * 1024); // 64M elements = 256 MB (exceeds L3)

} // namespace
} // namespace franklin

BENCHMARK_MAIN();
