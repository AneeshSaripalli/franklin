// Example 3: Float SIMD Operations
//
// Shows how to:
// - Generate code using float SIMD instructions (AVX2)
// - Use vaddps (float add) for float32 data
// - Compare with integer SIMD from example 2
//
// Build: bazel build -c opt //examples:asmjit_03_float_simd
// Run: bazel-bin/examples/asmjit_03_float_simd

#include <asmjit/x86.h>
#include <cmath>
#include <cstdio>

// Generated function: void add_float32x8(const float* a, const float* b, float*
// out)
using AddFloatFunc = void (*)(const float*, const float*, float*);

int main() {
  printf("=== Example 3: Float SIMD Operations ===\n\n");

  asmjit::JitRuntime rt;
  asmjit::CodeHolder code;
  code.init(rt.environment(), rt.cpu_features());
  asmjit::x86::Assembler a(&code);

  using namespace asmjit::x86;

  // Generate: void add_float32x8(const float* a, const float* b, float* out)
  // Args: rdi=a, rsi=b, rdx=out

  printf("Generating float SIMD kernel:\n");
  printf("  vmovups ymm0, [rdi]      ; Load 8 float32 from array a\n");
  printf("  vmovups ymm1, [rsi]      ; Load 8 float32 from array b\n");
  printf("  vaddps  ymm0, ymm0, ymm1 ; Float add (NOT vpaddd!)\n");
  printf("  vmovups [rdx], ymm0      ; Store 8 float32 to output\n");
  printf("  ret\n\n");

  // Load 8 float32 values from array a into ymm0
  a.vmovups(ymm0, ptr(rdi));

  // Load 8 float32 values from array b into ymm1
  a.vmovups(ymm1, ptr(rsi));

  // Float add: ymm0 = ymm0 + ymm1 (treats data as 8x float32)
  a.vaddps(ymm0, ymm0, ymm1);

  // Store result to output array
  a.vmovups(ptr(rdx), ymm0);

  a.ret();

  asmjit::Error err = a.finalize();
  if (err != asmjit::kErrorOk) {
    printf("ERROR: %s\n", asmjit::DebugUtils::error_as_string(err));
    return 1;
  }

  AddFloatFunc add_func;
  err = rt.add(&add_func, &code);
  if (err != asmjit::kErrorOk) {
    printf("ERROR: %s\n", asmjit::DebugUtils::error_as_string(err));
    return 1;
  }

  printf("Generated function at: %p\n\n", reinterpret_cast<void*>(add_func));

  // Test with arrays
  alignas(32) float a_data[8] = {1.5f, 2.5f, 3.5f, 4.5f,
                                 5.5f, 6.5f, 7.5f, 8.5f};
  alignas(32) float b_data[8] = {0.1f, 0.2f, 0.3f, 0.4f,
                                 0.5f, 0.6f, 0.7f, 0.8f};
  alignas(32) float result[8] = {0};

  printf("Input arrays:\n");
  printf("  a = [");
  for (int i = 0; i < 8; i++)
    printf("%.1f%s", a_data[i], i < 7 ? ", " : "]\n");
  printf("  b = [");
  for (int i = 0; i < 8; i++)
    printf("%.1f%s", b_data[i], i < 7 ? ", " : "]\n");

  // Call generated function
  add_func(a_data, b_data, result);

  printf("\nResult (float SIMD add):\n");
  printf("  result = [");
  for (int i = 0; i < 8; i++)
    printf("%.1f%s", result[i], i < 7 ? ", " : "]\n");

  // Verify
  printf("\nVerification:\n");
  bool correct = true;
  for (int i = 0; i < 8; i++) {
    float expected = a_data[i] + b_data[i];
    bool match = fabs(result[i] - expected) < 1e-6f;
    printf("  [%d] %.1f + %.1f = %.1f (expected %.1f) %s\n", i, a_data[i],
           b_data[i], result[i], expected, match ? "✓" : "✗");
    if (!match)
      correct = false;
  }

  printf("\n%s\n", correct ? "All tests passed!" : "TESTS FAILED!");

  return correct ? 0 : 1;
}
