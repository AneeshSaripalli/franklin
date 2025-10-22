#include "container/column.hpp"
#include <benchmark/benchmark.h>

namespace franklin {

// Benchmark BF16 WITHOUT bitmask operations
static void BM_BF16_Add_NoBitmask(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<BF16DefaultPolicy> a(size);
  column_vector<BF16DefaultPolicy> b(size);
  column_vector<BF16DefaultPolicy> result(size);

  // Fill with data
  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = bf16::from_float_trunc(static_cast<float>(i));
    b.data()[i] = bf16::from_float_trunc(static_cast<float>(i * 2));
  }

  size_t bytes_processed = 0;
  for (auto _ : state) {
    const bf16* a_ptr = a.data().data();
    const bf16* b_ptr = b.data().data();
    bf16* out_ptr = result.data().data();

    // Manually inline the operation WITHOUT bitmask loop
    for (size_t i = 0; i < size; i += 8) {
      auto a_storage =
          _mm_load_si128(reinterpret_cast<const __m128i*>(a_ptr + i));
      auto b_storage =
          _mm_load_si128(reinterpret_cast<const __m128i*>(b_ptr + i));

      auto a_compute = _mm256_cvtpbh_ps(reinterpret_cast<__m128bh>(a_storage));
      auto b_compute = _mm256_cvtpbh_ps(reinterpret_cast<__m128bh>(b_storage));

      auto result_compute = _mm256_add_ps(a_compute, b_compute);

      auto result_storage =
          reinterpret_cast<__m128i>(_mm256_cvtneps_pbh(result_compute));

      _mm_store_si128(reinterpret_cast<__m128i*>(out_ptr + i), result_storage);

      // NO bitmask loop here!
    }

    benchmark::DoNotOptimize(result);
    bytes_processed += size * sizeof(bf16) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// Benchmark Float32 WITHOUT bitmask operations
static void BM_Float32_Add_NoBitmask(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> b(size);
  column_vector<Float32DefaultPolicy> result(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<float>(i);
    b.data()[i] = static_cast<float>(i * 2);
  }

  size_t bytes_processed = 0;
  for (auto _ : state) {
    const float* a_ptr = a.data().data();
    const float* b_ptr = b.data().data();
    float* out_ptr = result.data().data();

    for (size_t i = 0; i < size; i += 8) {
      auto a_reg = _mm256_load_ps(a_ptr + i);
      auto b_reg = _mm256_load_ps(b_ptr + i);
      auto result_reg = _mm256_add_ps(a_reg, b_reg);
      _mm256_store_ps(out_ptr + i, result_reg);
      // NO bitmask loop!
    }

    benchmark::DoNotOptimize(result);
    bytes_processed += size * sizeof(float) * 3;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// Benchmark JUST the bitmask operations
static void BM_BitmaskOnly(benchmark::State& state) {
  const size_t size = state.range(0);
  std::vector<bool> a(size, true);
  std::vector<bool> b(size, true);
  std::vector<bool> result(size);

  size_t ops = 0;
  for (auto _ : state) {
    for (size_t i = 0; i < size; ++i) {
      result[i] = a[i] && b[i];
    }
    benchmark::DoNotOptimize(result);
    ops += size;
  }

  state.counters["ops/sec"] =
      benchmark::Counter(ops, benchmark::Counter::kIsRate);
}

// DRAM bandwidth baseline: 2 reads + 1 write (like element-wise ops)
// Use 64MB to ensure we're hitting DRAM (L3 is only 32MB)
static void BM_DRAM_Bandwidth_Baseline(benchmark::State& state) {
  const size_t size = state.range(0); // In elements

  // Allocate aligned buffers
  alignas(64) std::vector<float> a(size);
  alignas(64) std::vector<float> b(size);
  alignas(64) std::vector<float> result(size);

  // Initialize data
  for (size_t i = 0; i < size; ++i) {
    a[i] = static_cast<float>(i);
    b[i] = static_cast<float>(i * 2);
  }

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Simple load-add-store to match element-wise pattern
    float* a_ptr = a.data();
    float* b_ptr = b.data();
    float* out_ptr = result.data();

    for (size_t i = 0; i < size; i += 8) {
      __m256 a_reg = _mm256_loadu_ps(a_ptr + i);
      __m256 b_reg = _mm256_loadu_ps(b_ptr + i);
      __m256 result_reg = _mm256_add_ps(a_reg, b_reg);
      _mm256_storeu_ps(out_ptr + i, result_reg);
    }

    benchmark::DoNotOptimize(result);
    bytes_processed += size * sizeof(float) * 3; // 2 reads + 1 write
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);

  // Label based on size
  const size_t total_bytes = size * sizeof(float) * 2;
  if (total_bytes > 32 * 1024 * 1024) {
    state.SetLabel("DRAM");
  } else if (total_bytes > 8 * 1024 * 1024) {
    state.SetLabel("L3");
  }
}

BENCHMARK(BM_BF16_Add_NoBitmask)->Arg(4096)->Arg(1048576);
BENCHMARK(BM_Float32_Add_NoBitmask)->Arg(4096)->Arg(1048576);
BENCHMARK(BM_BitmaskOnly)->Arg(4096)->Arg(1048576);

// DRAM bandwidth tests: 2MB (L2), 8MB (L3 boundary), 16MB (DRAM), 64MB
// (definitely DRAM)
BENCHMARK(BM_DRAM_Bandwidth_Baseline)
    ->Arg(524288)    // 2MB (in L3)
    ->Arg(2097152)   // 8MB (L3 boundary)
    ->Arg(4194304)   // 16MB (DRAM)
    ->Arg(16777216); // 64MB (definitely DRAM)

} // namespace franklin

BENCHMARK_MAIN();
