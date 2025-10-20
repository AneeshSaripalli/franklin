// Benchmark: AsmJit-based runtime code generation vs branching
//
// This properly generates entire loop kernels using AsmJit, avoiding
// the function call overhead from the previous prototype.
//
// Build: bazel build -c opt //benchmarks:asmjit_kernel_benchmark
// Run: bazel-bin/benchmarks/asmjit_kernel_benchmark

#include <asmjit/x86.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <immintrin.h>
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

// AsmJit-based code generation: generates entire loop
using AddKernel = void (*)(const void* a, const void* b, float* out, size_t n);

class KernelGenerator {
public:
  KernelGenerator() { code_.init(rt_.environment(), rt_.cpu_features()); }

  AddKernel generate_add_kernel(DataType type_a, DataType type_b) {
    printf("Generating kernel for: %s + %s\n",
           type_a == DataType::Float32 ? "Float32" : "Int32",
           type_b == DataType::Float32 ? "Float32" : "Int32");

    code_.reset();
    code_.init(rt_.environment(), rt_.cpu_features());

    asmjit::x86::Assembler a(&code_);
    using namespace asmjit::x86;

    // Function signature: void(const void* a, const void* b, float* out, size_t
    // n) System V AMD64 ABI: rdi=a, rsi=b, rdx=out, rcx=n

    // Calculate loop end: rcx = n & ~7 (round down to multiple of 8)
    a.and_(rcx, ~7);
    a.test(rcx, rcx);

    asmjit::Label loop_end = a.new_label();
    a.jz(loop_end); // Skip if n < 8

    // Loop counter: r8 = 0
    a.xor_(r8, r8);

    asmjit::Label loop_start = a.new_label();
    a.bind(loop_start);

    // Load input A into ymm0
    if (type_a == DataType::Float32) {
      // vmovups ymm0, [rdi + r8*4]
      a.vmovups(ymm0, ptr(rdi, r8, 2)); // scale=2 means *4
    } else {
      // vcvtdq2ps ymm0, [rdi + r8*4]
      a.vcvtdq2ps(ymm0, ptr(rdi, r8, 2));
    }

    // Load input B into ymm1
    if (type_b == DataType::Float32) {
      // vmovups ymm1, [rsi + r8*4]
      a.vmovups(ymm1, ptr(rsi, r8, 2));
    } else {
      // vcvtdq2ps ymm1, [rsi + r8*4]
      a.vcvtdq2ps(ymm1, ptr(rsi, r8, 2));
    }

    // Add: vaddps ymm0, ymm0, ymm1
    a.vaddps(ymm0, ymm0, ymm1);

    // Store: vmovups [rdx + r8*4], ymm0
    a.vmovups(ptr(rdx, r8, 2), ymm0);

    // Increment loop counter: r8 += 8
    a.add(r8, 8);

    // Loop condition: if (r8 < rcx) goto loop_start
    a.cmp(r8, rcx);
    a.jl(loop_start);

    a.bind(loop_end);
    a.ret();

    // Finalize the code
    asmjit::Error err = a.finalize();
    if (err != asmjit::kErrorOk) {
      printf("AsmJit finalize error: %s\n",
             asmjit::DebugUtils::error_as_string(err));
      return nullptr;
    }

    // Add to runtime and get function pointer
    AddKernel func;
    err = rt_.add(&func, &code_);

    if (err != asmjit::kErrorOk) {
      printf("AsmJit error: %s\n", asmjit::DebugUtils::error_as_string(err));
      return nullptr;
    }

    printf("Generated kernel at %p\n", reinterpret_cast<void*>(func));
    return func;
  }

private:
  asmjit::JitRuntime rt_;
  asmjit::CodeHolder code_;
};

// Benchmark helper
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

  // Generate and benchmark AsmJit kernel
  KernelGenerator gen;
  AddKernel kernel = gen.generate_add_kernel(type_a, type_b);

  if (!kernel) {
    printf("ERROR: Failed to generate kernel\n");
    return;
  }

  // Verify correctness first
  kernel(a_ptr, b_ptr, result.data(), n);
  bool correct = true;
  for (size_t i = 0; i < std::min(size_t(8), n); i++) {
    float expected =
        (type_a == DataType::Float32 ? a_float[i]
                                     : static_cast<float>(a_int[i])) +
        (type_b == DataType::Float32 ? b_float[i]
                                     : static_cast<float>(b_int[i]));
    if (std::abs(result[i] - expected) > 0.01f) {
      printf("Verification failed at index %zu: got %.2f, expected %.2f\n", i,
             result[i], expected);
      correct = false;
    }
  }

  if (!correct) {
    printf("ERROR: Generated kernel produces incorrect results!\n");
    return;
  }

  auto asmjit_func = [&]() { kernel(a_ptr, b_ptr, result.data(), n); };

  double asmjit_time = benchmark_ms(asmjit_func, ITERATIONS);

  // Results
  double speedup = branch_time / asmjit_time;
  double improvement_pct = ((branch_time - asmjit_time) / branch_time) * 100.0;

  printf("\nResults (%d iterations):\n", ITERATIONS);
  printf("  Branching:  %.3f ms/iter (%.2f ns/elem)\n",
         branch_time / ITERATIONS, branch_time * 1e6 / (ITERATIONS * n));
  printf("  AsmJit:     %.3f ms/iter (%.2f ns/elem)\n",
         asmjit_time / ITERATIONS, asmjit_time * 1e6 / (ITERATIONS * n));
  printf("  Speedup:    %.2fx (%.1f%% improvement)\n", speedup,
         improvement_pct);
  printf("  Status:     %s\n", speedup > 1.0 ? "WIN ✓" : "LOSS ✗");
}

int main() {
  printf("==============================================\n");
  printf("AsmJit Code Generation vs Branching Benchmark\n");
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
