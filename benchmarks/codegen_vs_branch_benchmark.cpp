// Benchmark: Runtime code generation vs branching dispatch
//
// Compares performance of:
// 1. Runtime code generation (no branches in hot loop)
// 2. Switch-based dispatch (branch in hot loop)
//
// Build: g++ -std=c++20 -O3 -mavx2 -o bench codegen_vs_branch_benchmark.cpp
// Run: ./bench

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

enum class DataType { Int32, Float32 };

// Branching approach: dispatch inside hot loop
void add_with_branches(DataType type_a, DataType type_b, const void* a,
                       const void* b, float* out, size_t n) {
  for (size_t i = 0; i + 8 <= n; i += 8) {
    __m256 va, vb;

    // Branch #1: Load A
    if (type_a == DataType::Float32) {
      va = _mm256_loadu_ps(static_cast<const float*>(a) + i);
    } else {
      va = _mm256_cvtepi32_ps(
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
              static_cast<const int32_t*>(a) + i)));
    }

    // Branch #2: Load B
    if (type_b == DataType::Float32) {
      vb = _mm256_loadu_ps(static_cast<const float*>(b) + i);
    } else {
      vb = _mm256_cvtepi32_ps(
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
              static_cast<const int32_t*>(b) + i)));
    }

    // Add and store
    __m256 result = _mm256_add_ps(va, vb);
    _mm256_storeu_ps(out + i, result);
  }
}

// ===== Code generation approach (from previous prototype) =====

namespace Inst {
constexpr uint8_t LOAD_FP32_YMM0_RDI[] = {0xc5, 0xfc, 0x10, 0x07};
constexpr uint8_t LOAD_INT32_CVT_YMM0_RDI[] = {0xc5, 0xfc, 0x5b, 0x07};
constexpr uint8_t LOAD_FP32_YMM1_RSI[] = {0xc5, 0xfc, 0x10, 0x0e};
constexpr uint8_t LOAD_INT32_CVT_YMM1_RSI[] = {0xc5, 0xfc, 0x5b, 0x0e};
constexpr uint8_t ADD_YMM0_YMM1[] = {0xc5, 0xfc, 0x58, 0xc1};
constexpr uint8_t STORE_YMM0_RDX[] = {0xc5, 0xfc, 0x11, 0x02};
constexpr uint8_t RET[] = {0xc3};
} // namespace Inst

class ExecutableBuffer {
public:
  ExecutableBuffer() {
    size_t page_size = sysconf(_SC_PAGESIZE);
    buffer_ = mmap(nullptr, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buffer_ == MAP_FAILED) {
      buffer_ = nullptr;
      std::exit(1);
    }
    write_ptr_ = static_cast<uint8_t*>(buffer_);
  }

  ~ExecutableBuffer() {
    if (buffer_)
      munmap(buffer_, sysconf(_SC_PAGESIZE));
  }

  template <size_t N> void emit(const uint8_t (&bytes)[N]) {
    memcpy(write_ptr_, bytes, N);
    write_ptr_ += N;
  }

  void* get_function() {
    __builtin___clear_cache(buffer_, write_ptr_);
    return buffer_;
  }

private:
  void* buffer_;
  uint8_t* write_ptr_;
};

using AddKernel = void (*)(const void*, const void*, float*);
std::vector<ExecutableBuffer*> kernel_buffers;

AddKernel generate_add_kernel(DataType type_a, DataType type_b) {
  auto* buf = new ExecutableBuffer();
  kernel_buffers.push_back(buf);

  if (type_a == DataType::Float32) {
    buf->emit(Inst::LOAD_FP32_YMM0_RDI);
  } else {
    buf->emit(Inst::LOAD_INT32_CVT_YMM0_RDI);
  }

  if (type_b == DataType::Float32) {
    buf->emit(Inst::LOAD_FP32_YMM1_RSI);
  } else {
    buf->emit(Inst::LOAD_INT32_CVT_YMM1_RSI);
  }

  buf->emit(Inst::ADD_YMM0_YMM1);
  buf->emit(Inst::STORE_YMM0_RDX);
  buf->emit(Inst::RET);

  return reinterpret_cast<AddKernel>(buf->get_function());
}

void add_with_codegen(AddKernel kernel, const void* a, const void* b,
                      float* out, size_t n) {
  for (size_t i = 0; i + 8 <= n; i += 8) {
    kernel(static_cast<const uint8_t*>(a) + i * 4,
           static_cast<const uint8_t*>(b) + i * 4, out + i);
  }
}

// ===== Benchmark =====

double benchmark_ms(auto&& func, int iterations) {
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; i++) {
    func();
  }
  auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count();
}

void run_benchmark(const char* name, DataType type_a, DataType type_b,
                   size_t n) {
  printf("\n========================================\n");
  printf("Benchmark: %s\n", name);
  printf("Array size: %zu elements (%.1f KB)\n", n, n * 4.0 / 1024.0);
  printf("========================================\n");

  // Setup data
  std::vector<int32_t> a_int(n), b_int(n);
  std::vector<float> a_float(n), b_float(n), result(n);

  for (size_t i = 0; i < n; i++) {
    a_int[i] = static_cast<int32_t>(i);
    b_int[i] = static_cast<int32_t>(i * 2);
    a_float[i] = static_cast<float>(i);
    b_float[i] = static_cast<float>(i * 2);
  }

  const void* a_ptr = (type_a == DataType::Float32)
                          ? static_cast<const void*>(a_float.data())
                          : static_cast<const void*>(a_int.data());

  const void* b_ptr = (type_b == DataType::Float32)
                          ? static_cast<const void*>(b_float.data())
                          : static_cast<const void*>(b_int.data());

  const int ITERATIONS = 1000;

  // Benchmark branching version
  auto branch_func = [&]() {
    add_with_branches(type_a, type_b, a_ptr, b_ptr, result.data(), n);
  };

  double branch_time = benchmark_ms(branch_func, ITERATIONS);
  double branch_time_per_iter = branch_time / ITERATIONS;

  // Generate specialized kernel
  AddKernel kernel = generate_add_kernel(type_a, type_b);

  // Benchmark codegen version
  auto codegen_func = [&]() {
    add_with_codegen(kernel, a_ptr, b_ptr, result.data(), n);
  };

  double codegen_time = benchmark_ms(codegen_func, ITERATIONS);
  double codegen_time_per_iter = codegen_time / ITERATIONS;

  // Results
  double speedup = branch_time / codegen_time;
  double improvement_pct = ((branch_time - codegen_time) / branch_time) * 100.0;

  printf("\nResults (%d iterations):\n", ITERATIONS);
  printf("  Branching:  %.3f ms/iter\n", branch_time_per_iter);
  printf("  Code Gen:   %.3f ms/iter\n", codegen_time_per_iter);
  printf("  Speedup:    %.2fx (%.1f%% faster)\n", speedup, improvement_pct);
  printf("  Per element: %.2f ns (branch) vs %.2f ns (codegen)\n",
         branch_time_per_iter * 1e6 / n, codegen_time_per_iter * 1e6 / n);
}

int main() {
  printf("==============================================\n");
  printf("Code Generation vs Branching Benchmark\n");
  printf("==============================================\n");

  // Test different array sizes
  const size_t sizes[] = {
      8 * 1024,       // 32 KB (L1)
      256 * 1024,     // 1 MB (L2)
      8 * 1024 * 1024 // 32 MB (L3+)
  };

  for (size_t n : sizes) {
    run_benchmark("Float32 + Float32", DataType::Float32, DataType::Float32, n);
    run_benchmark("Int32 + Int32", DataType::Int32, DataType::Int32, n);
    run_benchmark("Int32 + Float32", DataType::Int32, DataType::Float32, n);
  }

  printf("\n==============================================\n");
  printf("Benchmark complete!\n");
  printf("==============================================\n");

  return 0;
}
