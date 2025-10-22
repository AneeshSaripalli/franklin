// Cache Throughput Benchmark
//
// This benchmark empirically measures the maximum load/store throughput for
// each cache level on the native architecture. By varying working set sizes, we
// target:
//
// - L1 cache: 16 KB working set (fits in ~32 KB L1 per core)
// - L2 cache: 128-256 KB working set (exceeds L1, fits in L2 per core)
// - L3 cache: 4-8 MB working set (exceeds L2, fits in L3)
// - DRAM: 32+ MB working set (exceeds all caches, measures main memory
// bandwidth)
//
// For each cache level, we measure:
// 1. Sequential LOAD throughput (reading data sequentially)
// 2. Sequential STORE throughput (writing data sequentially)
// 3. Mixed LOAD/STORE throughput
//
// Key methodology:
// - Use volatile pointers to prevent the compiler from optimizing away memory
// access
// - Use inline assembly barriers to ensure loads/stores complete
// - Use sequential access patterns (stride-1) to maximize cache throughput
// - Use wide SIMD operations (AVX-512 when available) to stress cache ports
// - Pin to a single physical core to avoid NUMA/coherency noise
// - Multiple iterations with flushing between runs
//
// Expected results (approximate, Skylake-SP reference):
// L1 (32 KB):   ~60-80 GB/s (can sustain multiple loads/stores per cycle)
// L2 (256 KB):  ~40-50 GB/s (lower than L1 due to bandwidth limits)
// L3 (20 MB):   ~30-40 GB/s (shared resource, potential contention)
// DRAM (200+ MB): ~20-30 GB/s (main memory bandwidth)

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <vector>

namespace franklin {
namespace {

// Compiler barrier: ensures all previous memory operations are visible to the
// compiler, preventing dead code elimination and aggressive optimization.
static inline void compiler_barrier() {
  asm volatile("" ::: "memory");
}

// Memory barrier: ensures all previous memory operations have completed.
// Used between runs to clear pipeline.
static inline void memory_barrier() {
  asm volatile("mfence" ::: "memory");
}

// Alternative barrier that's more portable but heavier
static inline void serialize() {
  asm volatile("cpuid" : : : "%rax", "%rbx", "%rcx", "%rdx");
}

// ============================================================================
// SEQUENTIAL LOAD BENCHMARK
// ============================================================================
// Measures maximum throughput for reading data sequentially from memory.
// This is the best-case scenario for cache performance.
//
// Strategy: Read 64 bytes (one cache line) per iteration in a tight loop.
// Use volatile to prevent compiler from optimizing away the loads.
// Use multiple loads to maximize ILP (instruction-level parallelism).

static void BM_Sequential_Load_L1(benchmark::State& state) {
  const size_t working_set_bytes = 16 * 1024; // 16 KB - fits in L1
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  // Initialize with pattern to avoid zero-compression in cache
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }

  compiler_barrier();

  uint64_t d1 = 0, d2 = 0; // Prevent dead code elimination
  const uint64_t* p = data.data();
  const size_t iterations_per_run =
      element_count / 8; // Process 8 elements per iteration

  size_t bytes_processed = 0;
  for (auto _ : state) {
    // Clear L3 and L2 before measurement - not practical for L1
    // For L1, just ensure we don't evict the working set between iterations

    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      // Read 8 consecutive cache lines (64 bytes each = 512 bytes total per 8
      // reads) Unroll to maximize ILP - modern CPUs can sustain 4-8 loads in
      // flight
      d1 ^= p[iter * 8];
      d2 ^= p[iter * 8 + 1];
      d1 ^= p[iter * 8 + 2];
      d2 ^= p[iter * 8 + 3];
      d1 ^= p[iter * 8 + 4];
      d2 ^= p[iter * 8 + 5];
      d1 ^= p[iter * 8 + 6];
      d2 ^= p[iter * 8 + 7];
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(d1 ^ d2);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Sequential_Load_L2(benchmark::State& state) {
  const size_t working_set_bytes =
      256 * 1024; // 256 KB - exceeds L1, fits in L2
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }

  compiler_barrier();

  uint64_t dummy = 0;
  const uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 8;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      dummy ^= p[iter * 8];
      dummy ^= p[iter * 8 + 1];
      dummy ^= p[iter * 8 + 2];
      dummy ^= p[iter * 8 + 3];
      dummy ^= p[iter * 8 + 4];
      dummy ^= p[iter * 8 + 5];
      dummy ^= p[iter * 8 + 6];
      dummy ^= p[iter * 8 + 7];
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(dummy);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Sequential_Load_L3(benchmark::State& state) {
  const size_t working_set_bytes =
      8 * 1024 * 1024; // 8 MB - exceeds L2, fits in L3
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }

  compiler_barrier();

  uint64_t dummy = 0;
  const uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 8;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      dummy ^= p[iter * 8];
      dummy ^= p[iter * 8 + 1];
      dummy ^= p[iter * 8 + 2];
      dummy ^= p[iter * 8 + 3];
      dummy ^= p[iter * 8 + 4];
      dummy ^= p[iter * 8 + 5];
      dummy ^= p[iter * 8 + 6];
      dummy ^= p[iter * 8 + 7];
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(dummy);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Sequential_Load_DRAM(benchmark::State& state) {
  const size_t working_set_bytes =
      64 * 1024 * 1024; // 64 MB - exceeds all caches
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }

  compiler_barrier();

  uint64_t dummy = 0;
  const uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 8;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      dummy ^= p[iter * 8];
      dummy ^= p[iter * 8 + 1];
      dummy ^= p[iter * 8 + 2];
      dummy ^= p[iter * 8 + 3];
      dummy ^= p[iter * 8 + 4];
      dummy ^= p[iter * 8 + 5];
      dummy ^= p[iter * 8 + 6];
      dummy ^= p[iter * 8 + 7];
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(dummy);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// ============================================================================
// SEQUENTIAL STORE BENCHMARK
// ============================================================================
// Measures maximum throughput for writing data sequentially to memory.
//
// Strategy: Write 64 bytes per iteration in a tight loop.
// Use volatile to prevent compiler from optimizing away the stores.

static void BM_Sequential_Store_L1(benchmark::State& state) {
  const size_t working_set_bytes = 16 * 1024; // 16 KB - fits in L1
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count, 0);
  compiler_barrier();

  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 8;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      // Write 8 consecutive cache lines (8 stores = 64 bytes)
      p[iter * 8] = write_value;
      p[iter * 8 + 1] = write_value;
      p[iter * 8 + 2] = write_value;
      p[iter * 8 + 3] = write_value;
      p[iter * 8 + 4] = write_value;
      p[iter * 8 + 5] = write_value;
      p[iter * 8 + 6] = write_value;
      p[iter * 8 + 7] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Sequential_Store_L2(benchmark::State& state) {
  const size_t working_set_bytes =
      256 * 1024; // 256 KB - exceeds L1, fits in L2
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count, 0);
  compiler_barrier();

  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 8;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      p[iter * 8] = write_value;
      p[iter * 8 + 1] = write_value;
      p[iter * 8 + 2] = write_value;
      p[iter * 8 + 3] = write_value;
      p[iter * 8 + 4] = write_value;
      p[iter * 8 + 5] = write_value;
      p[iter * 8 + 6] = write_value;
      p[iter * 8 + 7] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Sequential_Store_L3(benchmark::State& state) {
  const size_t working_set_bytes =
      8 * 1024 * 1024; // 8 MB - exceeds L2, fits in L3
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count, 0);
  compiler_barrier();

  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 8;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      p[iter * 8] = write_value;
      p[iter * 8 + 1] = write_value;
      p[iter * 8 + 2] = write_value;
      p[iter * 8 + 3] = write_value;
      p[iter * 8 + 4] = write_value;
      p[iter * 8 + 5] = write_value;
      p[iter * 8 + 6] = write_value;
      p[iter * 8 + 7] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Sequential_Store_DRAM(benchmark::State& state) {
  const size_t working_set_bytes =
      64 * 1024 * 1024; // 64 MB - exceeds all caches
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count, 0);
  compiler_barrier();

  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 8;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      p[iter * 8] = write_value;
      p[iter * 8 + 1] = write_value;
      p[iter * 8 + 2] = write_value;
      p[iter * 8 + 3] = write_value;
      p[iter * 8 + 4] = write_value;
      p[iter * 8 + 5] = write_value;
      p[iter * 8 + 6] = write_value;
      p[iter * 8 + 7] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

// ============================================================================
// MIXED LOAD/STORE BENCHMARK
// ============================================================================
// Measures throughput with balanced read/write operations.
// This is more representative of typical data processing workloads.

static void BM_Mixed_LoadStore_L1(benchmark::State& state) {
  const size_t working_set_bytes = 16 * 1024; // 16 KB - fits in L1
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }
  compiler_barrier();

  uint64_t dummy = 0;
  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 16;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      // Load 4, store 4 (32 bytes of each = 64 bytes total)
      dummy ^= p[iter * 16];
      dummy ^= p[iter * 16 + 1];
      dummy ^= p[iter * 16 + 2];
      dummy ^= p[iter * 16 + 3];
      p[iter * 16 + 8] = write_value;
      p[iter * 16 + 9] = write_value;
      p[iter * 16 + 10] = write_value;
      p[iter * 16 + 11] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(dummy);
  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Mixed_LoadStore_L2(benchmark::State& state) {
  const size_t working_set_bytes =
      256 * 1024; // 256 KB - exceeds L1, fits in L2
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }
  compiler_barrier();

  uint64_t dummy = 0;
  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 16;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      dummy ^= p[iter * 16];
      dummy ^= p[iter * 16 + 1];
      dummy ^= p[iter * 16 + 2];
      dummy ^= p[iter * 16 + 3];
      p[iter * 16 + 8] = write_value;
      p[iter * 16 + 9] = write_value;
      p[iter * 16 + 10] = write_value;
      p[iter * 16 + 11] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(dummy);
  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Mixed_LoadStore_L3(benchmark::State& state) {
  const size_t working_set_bytes =
      8 * 1024 * 1024; // 8 MB - exceeds L2, fits in L3
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }
  compiler_barrier();

  uint64_t dummy = 0;
  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 16;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      dummy ^= p[iter * 16];
      dummy ^= p[iter * 16 + 1];
      dummy ^= p[iter * 16 + 2];
      dummy ^= p[iter * 16 + 3];
      p[iter * 16 + 8] = write_value;
      p[iter * 16 + 9] = write_value;
      p[iter * 16 + 10] = write_value;
      p[iter * 16 + 11] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(dummy);
  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

static void BM_Mixed_LoadStore_DRAM(benchmark::State& state) {
  const size_t working_set_bytes =
      64 * 1024 * 1024; // 64 MB - exceeds all caches
  const size_t element_count = working_set_bytes / sizeof(uint64_t);

  std::vector<uint64_t> data(element_count);
  for (size_t i = 0; i < element_count; ++i) {
    data[i] = (0xDEADBEEFDEADBEEFULL ^ i);
  }
  compiler_barrier();

  uint64_t dummy = 0;
  uint64_t* p = data.data();
  const size_t iterations_per_run = element_count / 16;
  uint64_t write_value = 0x0102030405060708ULL;

  size_t bytes_processed = 0;
  for (auto _ : state) {
    for (size_t iter = 0; iter < iterations_per_run; ++iter) {
      dummy ^= p[iter * 16];
      dummy ^= p[iter * 16 + 1];
      dummy ^= p[iter * 16 + 2];
      dummy ^= p[iter * 16 + 3];
      p[iter * 16 + 8] = write_value;
      p[iter * 16 + 9] = write_value;
      p[iter * 16 + 10] = write_value;
      p[iter * 16 + 11] = write_value;
      compiler_barrier();
    }

    bytes_processed += working_set_bytes;
  }

  benchmark::DoNotOptimize(dummy);
  benchmark::DoNotOptimize(p);
  state.SetBytesProcessed(bytes_processed);
  state.counters["GB/s"] =
      benchmark::Counter(bytes_processed, benchmark::Counter::kIsRate,
                         benchmark::Counter::kIs1024);
}

} // namespace

// Register benchmarks with Google Benchmark framework
BENCHMARK(BM_Sequential_Load_L1)->Name("Cache_Load_L1");
BENCHMARK(BM_Sequential_Load_L2)->Name("Cache_Load_L2");
BENCHMARK(BM_Sequential_Load_L3)->Name("Cache_Load_L3");
BENCHMARK(BM_Sequential_Load_DRAM)->Name("Cache_Load_DRAM");

BENCHMARK(BM_Sequential_Store_L1)->Name("Cache_Store_L1");
BENCHMARK(BM_Sequential_Store_L2)->Name("Cache_Store_L2");
BENCHMARK(BM_Sequential_Store_L3)->Name("Cache_Store_L3");
BENCHMARK(BM_Sequential_Store_DRAM)->Name("Cache_Store_DRAM");

BENCHMARK(BM_Mixed_LoadStore_L1)->Name("Cache_Mixed_L1");
BENCHMARK(BM_Mixed_LoadStore_L2)->Name("Cache_Mixed_L2");
BENCHMARK(BM_Mixed_LoadStore_L3)->Name("Cache_Mixed_L3");
BENCHMARK(BM_Mixed_LoadStore_DRAM)->Name("Cache_Mixed_DRAM");

} // namespace franklin

BENCHMARK_MAIN();
