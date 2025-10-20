// Example 1: Basic Function Generation
//
// Shows how to:
// - Create a JIT runtime
// - Generate a simple function that adds two integers
// - Call the generated function
//
// Build: bazel build -c opt //examples:asmjit_01_basic_function
// Run: bazel-bin/examples/asmjit_01_basic_function

#include <asmjit/x86.h>
#include <cstdio>

// Function signature we're generating: int32_t add(int32_t a, int32_t b)
using AddFunc = int32_t (*)(int32_t, int32_t);

int main() {
  printf("=== Example 1: Basic Function Generation ===\n\n");

  // Step 1: Create JIT runtime (manages executable memory)
  asmjit::JitRuntime rt;
  printf("Created JIT runtime\n");

  // Step 2: Create code holder (container for generated code)
  asmjit::CodeHolder code;
  code.init(rt.environment(), rt.cpu_features());
  printf("Initialized code holder\n");

  // Step 3: Create x86 assembler
  asmjit::x86::Assembler a(&code);
  printf("Created assembler\n\n");

  // Step 4: Generate code
  // Function: int32_t add(int32_t a, int32_t b) { return a + b; }
  //
  // System V AMD64 calling convention:
  //   First arg (a) in: edi (32-bit portion of rdi)
  //   Second arg (b) in: esi (32-bit portion of rsi)
  //   Return value in: eax

  printf("Generating code:\n");
  printf("  add eax, edi  ; eax = 0 + a\n");
  printf("  add eax, esi  ; eax = a + b\n");
  printf("  ret           ; return eax\n\n");

  // Move first argument to return register
  a.mov(asmjit::x86::eax, asmjit::x86::edi); // eax = a

  // Add second argument
  a.add(asmjit::x86::eax, asmjit::x86::esi); // eax = a + b

  // Return
  a.ret();

  // Step 5: Finalize the assembler
  asmjit::Error err = a.finalize();
  if (err != asmjit::kErrorOk) {
    printf("ERROR: Finalize failed: %s\n",
           asmjit::DebugUtils::error_as_string(err));
    return 1;
  }
  printf("Finalized assembler\n");

  // Step 6: Add to runtime and get function pointer
  AddFunc add_func;
  err = rt.add(&add_func, &code);
  if (err != asmjit::kErrorOk) {
    printf("ERROR: Runtime add failed: %s\n",
           asmjit::DebugUtils::error_as_string(err));
    return 1;
  }
  printf("Generated function at: %p\n\n", reinterpret_cast<void*>(add_func));

  // Step 7: Test the generated function
  printf("Testing generated function:\n");

  int32_t test_cases[][3] = {{5, 3, 8},
                             {100, 200, 300},
                             {-10, 25, 15},
                             {0, 0, 0},
                             {1000000, 2000000, 3000000}};

  for (auto& test : test_cases) {
    int32_t a = test[0];
    int32_t b = test[1];
    int32_t expected = test[2];
    int32_t result = add_func(a, b);

    printf("  add(%d, %d) = %d (expected %d) %s\n", a, b, result, expected,
           result == expected ? "✓" : "✗");
  }

  printf("\nSuccess! Generated and executed JIT code.\n");

  // Runtime will automatically free the generated code when it goes out of
  // scope
  return 0;
}
