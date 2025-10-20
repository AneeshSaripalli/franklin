// Example 4: Type Conversion in SIMD
//
// Shows how to:
// - Convert int32 -> float32 using vcvtdq2ps
// - Convert float32 -> int32 using vcvttps2dq
// - Understand when conversions are necessary vs wasteful
//
// Build: bazel build -c opt //examples:asmjit_04_type_conversion
// Run: bazel-bin/examples/asmjit_04_type_conversion

#include <asmjit/x86.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

// Function 1: int32 + float32 -> float32 (needs conversion)
using MixedAddFunc = void (*)(const int32_t*, const float*, float*);

// Function 2: int32 + int32 -> float32 (wasteful conversion!)
using WastefulFunc = void (*)(const int32_t*, const int32_t*, float*);

void generate_mixed_add(asmjit::JitRuntime& rt, MixedAddFunc* out_func) {
  printf("=== Generating: int32 + float32 -> float32 ===\n");
  printf("This NEEDS conversion since inputs are different types.\n\n");

  asmjit::CodeHolder code;
  code.init(rt.environment(), rt.cpu_features());
  asmjit::x86::Assembler a(&code);

  using namespace asmjit::x86;

  // Args: rdi=int32_array, rsi=float_array, rdx=output

  printf("  vmovdqu    ymm0, [rdi]       ; Load 8 int32\n");
  printf("  vcvtdq2ps  ymm0, ymm0         ; Convert int32 -> float32\n");
  printf("  vmovups    ymm1, [rsi]       ; Load 8 float32\n");
  printf("  vaddps     ymm0, ymm0, ymm1  ; Float add\n");
  printf("  vmovups    [rdx], ymm0       ; Store result\n");
  printf("  ret\n\n");

  a.vmovdqu(ymm0, ptr(rdi));  // Load int32
  a.vcvtdq2ps(ymm0, ymm0);    // Convert to float
  a.vmovups(ymm1, ptr(rsi));  // Load float
  a.vaddps(ymm0, ymm0, ymm1); // Add in float domain
  a.vmovups(ptr(rdx), ymm0);  // Store
  a.ret();

  a.finalize();
  rt.add(out_func, &code);

  printf("Generated at: %p\n\n", reinterpret_cast<void*>(*out_func));
}

void generate_wasteful_conversion(asmjit::JitRuntime& rt,
                                  WastefulFunc* out_func) {
  printf("=== Generating: int32 + int32 -> float32 (WASTEFUL!) ===\n");
  printf("This converts both inputs to float unnecessarily.\n");
  printf("Better approach: use integer add (vpaddd), then convert result.\n\n");

  asmjit::CodeHolder code;
  code.init(rt.environment(), rt.cpu_features());
  asmjit::x86::Assembler a(&code);

  using namespace asmjit::x86;

  printf("  vmovdqu    ymm0, [rdi]       ; Load 8 int32 (a)\n");
  printf("  vcvtdq2ps  ymm0, ymm0         ; Convert a to float (wasteful!)\n");
  printf("  vmovdqu    ymm1, [rsi]       ; Load 8 int32 (b)\n");
  printf("  vcvtdq2ps  ymm1, ymm1         ; Convert b to float (wasteful!)\n");
  printf("  vaddps     ymm0, ymm0, ymm1  ; Float add (wasteful!)\n");
  printf("  vmovups    [rdx], ymm0       ; Store result\n");
  printf("  ret\n\n");

  a.vmovdqu(ymm0, ptr(rdi));
  a.vcvtdq2ps(ymm0, ymm0); // Convert a
  a.vmovdqu(ymm1, ptr(rsi));
  a.vcvtdq2ps(ymm1, ymm1);    // Convert b
  a.vaddps(ymm0, ymm0, ymm1); // Float add
  a.vmovups(ptr(rdx), ymm0);
  a.ret();

  a.finalize();
  rt.add(out_func, &code);

  printf("Generated at: %p\n\n", reinterpret_cast<void*>(*out_func));
}

int main() {
  printf("=== Example 4: Type Conversion Comparison ===\n\n");

  asmjit::JitRuntime rt;

  // Generate both kernels
  MixedAddFunc mixed_func;
  WastefulFunc wasteful_func;

  generate_mixed_add(rt, &mixed_func);
  generate_wasteful_conversion(rt, &wasteful_func);

  // Test 1: Mixed types (int32 + float32)
  {
    printf("========================================\n");
    printf("Test 1: int32 + float32 -> float32\n");
    printf("========================================\n");

    alignas(32) int32_t a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    alignas(32) float b[8] = {0.5f, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f};
    alignas(32) float result[8];

    mixed_func(a, b, result);

    printf("\nInput:\n");
    printf("  int32_array = [%d, %d, %d, %d, %d, %d, %d, %d]\n", a[0], a[1],
           a[2], a[3], a[4], a[5], a[6], a[7]);
    printf("  float_array = [%.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f]\n",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);

    printf("\nOutput:\n");
    printf("  result = [");
    for (int i = 0; i < 8; i++)
      printf("%.1f%s", result[i], i < 7 ? ", " : "]\n");

    printf("\nVerification:\n");
    for (int i = 0; i < 4; i++) {
      float expected = static_cast<float>(a[i]) + b[i];
      printf("  [%d] %d + %.1f = %.1f ✓\n", i, a[i], b[i], result[i]);
    }
  }

  // Test 2: Both int32 (wasteful)
  {
    printf("\n========================================\n");
    printf("Test 2: int32 + int32 -> float32 (WASTEFUL)\n");
    printf("========================================\n");

    alignas(32) int32_t a[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    alignas(32) int32_t b[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    alignas(32) float result[8];

    wasteful_func(a, b, result);

    printf("\nInput:\n");
    printf("  int32_array_a = [%d, %d, %d, %d, %d, %d, %d, %d]\n", a[0], a[1],
           a[2], a[3], a[4], a[5], a[6], a[7]);
    printf("  int32_array_b = [%d, %d, %d, %d, %d, %d, %d, %d]\n", b[0], b[1],
           b[2], b[3], b[4], b[5], b[6], b[7]);

    printf("\nOutput:\n");
    printf("  result = [");
    for (int i = 0; i < 8; i++)
      printf("%.0f%s", result[i], i < 7 ? ", " : "]\n");

    printf("\nThis works, but...\n");
    printf("BETTER: Use vpaddd (int add), then vcvtdq2ps once if needed!\n");
    printf("  vpaddd ymm0, [rdi], [rsi]  ; Integer add (no conversion!)\n");
    printf("  vcvtdq2ps ymm0, ymm0       ; Convert result to float (1 "
           "conversion)\n");
  }

  printf("\n========================================\n");
  printf("KEY INSIGHT\n");
  printf("========================================\n");
  printf("For int32 + int32:\n");
  printf("  - Output int32:   Use vpaddd (NO conversion)\n");
  printf("  - Output float32: vpaddd then vcvtdq2ps (1 conversion)\n");
  printf("  - NEVER:          Convert both inputs! (2 conversions - "
         "wasteful)\n\n");
  printf("This is why you need compute-type-aware code generation!\n");

  return 0;
}
