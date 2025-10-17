#include "container/column.hpp"
#include <benchmark/benchmark.h>

namespace franklin {

// Microbenchmark: Just BF16 loads and stores (no compute)
static void BM_BF16_LoadStore_Only(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<BF16DefaultPolicy> a(size);
  column_vector<BF16DefaultPolicy> result(size);

  // Fill with data
  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i));
  }

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Just copy: load + store, no conversion
    std::memcpy(result.data().data(), a.data().data(), size * sizeof(bf16));
    benchmark::DoNotOptimize(result);
    bytes_processed += size * sizeof(bf16) * 2; // read + write
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// Microbenchmark: BF16 with conversion but no compute
static void BM_BF16_ConvertOnly(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<BF16DefaultPolicy> a(size);
  column_vector<BF16DefaultPolicy> result(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = bf16::from_float(static_cast<float>(i));
  }

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Convert bf16 -> fp32 -> bf16
    const bf16* src = a.data().data();
    bf16* dst = result.data().data();

    for (size_t i = 0; i < size; i += 8) {
      __m128i bf16_data =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
      __m256 fp32_data = _mm256_cvtpbh_ps((__m128bh)bf16_data);
      __m128i bf16_result = (__m128i)_mm256_cvtneps_pbh(fp32_data);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), bf16_result);
    }

    benchmark::DoNotOptimize(result);
    bytes_processed += size * sizeof(bf16) * 2;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// Microbenchmark: Float32 for comparison
static void BM_Float32_LoadStore_Only(benchmark::State& state) {
  const size_t size = state.range(0);
  column_vector<Float32DefaultPolicy> a(size);
  column_vector<Float32DefaultPolicy> result(size);

  for (size_t i = 0; i < size; ++i) {
    a.data()[i] = static_cast<float>(i);
  }

  size_t bytes_processed = 0;
  for (auto _ : state) {
    std::memcpy(result.data().data(), a.data().data(), size * sizeof(float));
    benchmark::DoNotOptimize(result);
    bytes_processed += size * sizeof(float) * 2;
  }

  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

BENCHMARK(BM_BF16_LoadStore_Only)->Arg(4096)->Arg(1048576);
BENCHMARK(BM_BF16_ConvertOnly)->Arg(4096)->Arg(1048576);
BENCHMARK(BM_Float32_LoadStore_Only)->Arg(4096)->Arg(1048576);

} // namespace franklin

BENCHMARK_MAIN();
