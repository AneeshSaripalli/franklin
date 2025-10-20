// Example 2: Integer SIMD Operations
//
// Shows how to:
// - Generate code using integer SIMD instructions (AVX2)
// - Load/store with ymm registers for int32
// - Use vpaddd (integer add) vs vaddps (float add)
//
// Build: bazel build -c opt //examples:asmjit_02_integer_simd
// Run: bazel-bin/examples/asmjit_02_integer_simd

#include <asmjit/x86.h>
#include <cstdint>
#include <cstdio>

// Generated function: void add_int32x8(const int32_t* a, const int32_t* b,
// int32_t* out)
using AddInt32Func = void (*)(const int32_t*, const int32_t*, int32_t*);

int main() {
  printf("=== Example 2: Integer SIMD Operations ===\n\n");

  asmjit::JitRuntime rt;
  asmjit::CodeHolder code;
  code.init(rt.environment(), rt.cpu_features());
  asmjit::x86::Assembler a(&code);

  using namespace asmjit::x86;

  // Generate: void add_int32x8(const int32_t* a, const int32_t* b, int32_t*
  // out) Args: rdi=a, rsi=b, rdx=out
  //
  // We'll load 8 int32 values (256 bits = ymm register size)

  printf("Generating integer SIMD kernel:\n");
  printf("  vmovdqu ymm0, [rdi]     ; Load 8 int32 from array a\n");
  printf("  vmovdqu ymm1, [rsi]     ; Load 8 int32 from array b\n");
  printf("  vpaddd  ymm0, ymm0, ymm1 ; Integer add (NOT vaddps!)\n");
  printf("  vmovdqu [rdx], ymm0      ; Store 8 int32 to output\n");
  printf("  ret\n\n");

  // Load 8 int32 values from array a into ymm0
  a.vmovdqu(ymm0, ptr(rdi));

  // Load 8 int32 values from array b into ymm1
  a.vmovdqu(ymm1, ptr(rsi));

  // Integer add: ymm0 = ymm0 + ymm1 (treats data as 8x int32)
  a.vpaddd(ymm0, ymm0, ymm1);

  // Store result to output array
  a.vmovdqu(ptr(rdx), ymm0);

  a.ret();

  asmjit::Error err = a.finalize();
  if (err != asmjit::kErrorOk) {
    printf("ERROR: %s\n", asmjit::DebugUtils::error_as_string(err));
    return 1;
  }

  AddInt32Func add_func;
  err = rt.add(&add_func, &code);
  if (err != asmjit::kErrorOk) {
    printf("ERROR: %s\n", asmjit::DebugUtils::error_as_string(err));
    return 1;
  }

  printf("Generated function at: %p\n\n", reinterpret_cast<void*>(add_func));

  // Test with arrays
  alignas(32) int32_t a_data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  alignas(32) int32_t b_data[8] = {10, 20, 30, 40, 50, 60, 70, 80};
  alignas(32) int32_t result[8] = {0};

  printf("Input arrays:\n");
  printf("  a = [");
  for (int i = 0; i < 8; i++)
    printf("%d%s", a_data[i], i < 7 ? ", " : "]\n");
  printf("  b = [");
  for (int i = 0; i < 8; i++)
    printf("%d%s", b_data[i], i < 7 ? ", " : "]\n");

  // Call generated function
  add_func(a_data, b_data, result);

  printf("\nResult (integer SIMD add):\n");
  printf("  result = [");
  for (int i = 0; i < 8; i++)
    printf("%d%s", result[i], i < 7 ? ", " : "]\n");

  // Verify
  printf("\nVerification:\n");
  bool correct = true;
  for (int i = 0; i < 8; i++) {
    int32_t expected = a_data[i] + b_data[i];
    printf("  [%d] %d + %d = %d (expected %d) %s\n", i, a_data[i], b_data[i],
           result[i], expected, result[i] == expected ? "✓" : "✗");
    if (result[i] != expected)
      correct = false;
  }

  printf("\n%s\n", correct ? "All tests passed!" : "TESTS FAILED!");

  // Important note
  printf("\n=== Key Point ===\n");
  printf("We used 'vpaddd' (integer add), NOT 'vaddps' (float add)!\n");
  printf("This preserves full 32-bit integer precision without conversion.\n");

  return correct ? 0 : 1;
}
