// Example 5: Generating Loops
//
// Shows how to:
// - Create labels for loop targets
// - Use conditional jumps (jl, jz, etc.)
// - Generate efficient vectorized loops
// - Handle loop counters and bounds
//
// Build: bazel build -c opt //examples:asmjit_05_loops
// Run: bazel-bin/examples/asmjit_05_loops

#include <asmjit/x86.h>
#include <cstdint>
#include <cstdio>
#include <vector>

// Generated function: void vector_add(const int32_t* a, const int32_t* b,
// int32_t* out, size_t n)
using VectorAddFunc = void (*)(const int32_t*, const int32_t*, int32_t*,
                               size_t);

int main() {
  printf("=== Example 5: Generating Loops ===\n\n");

  asmjit::JitRuntime rt;
  asmjit::CodeHolder code;
  code.init(rt.environment(), rt.cpu_features());
  asmjit::x86::Assembler a(&code);

  using namespace asmjit::x86;

  // Function signature: void vector_add(const int32_t* a, const int32_t* b,
  // int32_t* out, size_t n) System V AMD64 ABI:
  //   rdi = a
  //   rsi = b
  //   rdx = out
  //   rcx = n (element count)

  printf("Generating vectorized loop:\n");
  printf("  // Calculate loop bound: n_vectors = n / 8\n");
  printf("  // Use r8 as loop counter\n\n");

  printf("Code structure:\n");
  printf("  mov r9, rcx           ; Save original n\n");
  printf(
      "  shr rcx, 3            ; rcx = n / 8 (number of 8-element vectors)\n");
  printf("  test rcx, rcx         ; Check if n >= 8\n");
  printf("  jz scalar_loop        ; If n < 8, skip vector loop\n\n");

  printf("  xor r8, r8            ; r8 = 0 (loop counter)\n\n");

  printf("vector_loop:\n");
  printf("  vmovdqu ymm0, [rdi + r8*4]  ; Load 8 int32 from a\n");
  printf("  vpaddd ymm0, ymm0, [rsi + r8*4]  ; Add 8 int32 from b\n");
  printf("  vmovdqu [rdx + r8*4], ymm0  ; Store result\n");
  printf("  add r8, 8             ; i += 8\n");
  printf("  cmp r8, r9            ; Compare with original n\n");
  printf("  jl vector_loop        ; Loop while i < n\n\n");

  printf("scalar_loop:\n");
  printf("  ; Handle remaining elements (n %% 8)\n");
  printf("  ret\n\n");

  // Save original n in r9
  a.mov(r9, rcx);

  // Calculate number of full vectors: rcx = n / 8
  a.shr(rcx, 3);

  // Check if we have at least one vector
  a.test(rcx, rcx);

  // Create labels
  asmjit::Label vector_loop = a.new_label();
  asmjit::Label scalar_loop = a.new_label();
  asmjit::Label done = a.new_label();

  // If n < 8, skip to scalar handling
  a.jz(scalar_loop);

  // Initialize loop counter
  a.xor_(r8, r8);

  // Start of vector loop
  a.bind(vector_loop);

  // Load from a[i*4] (i is in r8, each element is 4 bytes)
  // ptr(base_reg, index_reg, shift) generates [base + index << shift]
  // shift=2 means multiply by 4 (for int32)
  a.vmovdqu(ymm0, ptr(rdi, r8, 2));

  // Add from b[i*4]
  a.vpaddd(ymm0, ymm0, ptr(rsi, r8, 2));

  // Store to out[i*4]
  a.vmovdqu(ptr(rdx, r8, 2), ymm0);

  // Increment counter by 8
  a.add(r8, 8);

  // Loop condition: continue while r8 < r9
  a.cmp(r8, r9);
  a.jl(vector_loop);

  // Scalar loop (for tail elements)
  a.bind(scalar_loop);
  // For simplicity, we'll skip scalar handling in this example
  // In production, you'd loop through remaining elements here

  a.bind(done);
  a.ret();

  asmjit::Error err = a.finalize();
  if (err != asmjit::kErrorOk) {
    printf("ERROR: %s\n", asmjit::DebugUtils::error_as_string(err));
    return 1;
  }

  VectorAddFunc func;
  err = rt.add(&func, &code);
  if (err != asmjit::kErrorOk) {
    printf("ERROR: %s\n", asmjit::DebugUtils::error_as_string(err));
    return 1;
  }

  printf("Generated function at: %p\n\n", reinterpret_cast<void*>(func));

  // Test with various sizes
  struct TestCase {
    size_t n;
    const char* description;
  };

  TestCase tests[] = {
      {8, "Exactly 1 vector"},
      {16, "Exactly 2 vectors"},
      {24, "Exactly 3 vectors"},
      {32, "Exactly 4 vectors"},
  };

  for (const auto& test : tests) {
    printf("========================================\n");
    printf("Test: %s (n=%zu)\n", test.description, test.n);
    printf("========================================\n");

    std::vector<int32_t> a(test.n);
    std::vector<int32_t> b(test.n);
    std::vector<int32_t> result(test.n, 0);

    // Fill with test data
    for (size_t i = 0; i < test.n; i++) {
      a[i] = static_cast<int32_t>(i);
      b[i] = static_cast<int32_t>(i * 10);
    }

    // Call generated function
    func(a.data(), b.data(), result.data(), test.n);

    // Verify first 8 elements
    printf("First 8 results:\n");
    bool correct = true;
    for (size_t i = 0; i < std::min(size_t(8), test.n); i++) {
      int32_t expected = a[i] + b[i];
      bool match = result[i] == expected;
      printf("  [%zu] %d + %d = %d (expected %d) %s\n", i, a[i], b[i],
             result[i], expected, match ? "✓" : "✗");
      if (!match)
        correct = false;
    }

    printf("%s\n\n", correct ? "PASS" : "FAIL");
  }

  printf("=== Loop Generation Complete ===\n");

  return 0;
}
