// Simplified runtime code patching prototype
//
// Instead of patching existing code, we generate a small code buffer at runtime
// that contains the specialized load instructions, then call it.
//
// Build: g++ -std=c++20 -O2 -mavx2 -o patcher2 runtime_code_patcher_v2.cpp
// Run: ./patcher2

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

enum class DataType { Int32, Float32 };

// Get instruction bytes for loading 8 elements and converting to __m256
std::vector<uint8_t> get_load_instruction(DataType type) {
  switch (type) {
  case DataType::Float32:
    // vmovups ymm0, [rdi]  (load 8 floats from first arg)
    return {0xc5, 0xfc, 0x10, 0x07};

  case DataType::Int32:
    // vcvtdq2ps ymm0, [rdi]  (load 8 int32 and convert to float)
    return {0xc5, 0xfe, 0x5b, 0x07};
  }
  return {};
}

// Simple code generator for add kernel
class CodeGenerator {
public:
  CodeGenerator() {
    // Allocate executable memory
    size_t page_size = sysconf(_SC_PAGESIZE);
    code_buffer_ = mmap(nullptr, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (code_buffer_ == MAP_FAILED) {
      perror("mmap failed");
      code_buffer_ = nullptr;
    }

    code_ptr_ = static_cast<uint8_t*>(code_buffer_);
  }

  ~CodeGenerator() {
    if (code_buffer_) {
      munmap(code_buffer_, sysconf(_SC_PAGESIZE));
    }
  }

  // Generate specialized kernel: load a (type_a), load b (type_b), add, store
  using KernelFunc = void (*)(const void* a, const void* b, float* out);

  KernelFunc generate_add_kernel(DataType type_a, DataType type_b) {
    uint8_t* start = code_ptr_;

    printf("Generating kernel for %s + %s\n",
           type_a == DataType::Float32 ? "Float32" : "Int32",
           type_b == DataType::Float32 ? "Float32" : "Int32");

    // Function prologue (optional, for debugging)
    // We're using System V AMD64 ABI:
    //   rdi = a, rsi = b, rdx = out

    // Load input A into ymm0
    auto load_a = get_load_instruction(type_a);
    // rdi already points to input A
    emit_bytes(load_a);

    // Load input B into ymm1
    auto load_b = get_load_instruction(type_b);
    // Need to move rsi to rdi for the load
    emit_bytes({0x48, 0x89, 0xf7}); // mov rdi, rsi
    emit_bytes(load_b);
    emit_bytes(
        {0xc4, 0xc1, 0x7c, 0x28, 0xc8}); // vmovaps ymm1, ymm0 (move to ymm1)

    // Restore rdi (first arg)
    // Actually, let's use a different approach - use [rsi] directly for second
    // load

    // Let me restart with correct approach
    code_ptr_ = start; // Reset

    // Load A: vmovups ymm0, [rdi]  OR vcvtdq2ps ymm0, [rdi]
    emit_bytes(get_load_instruction(type_a));

    // Load B: vmovups ymm1, [rsi]  OR vcvtdq2ps ymm1, [rsi]
    // Modify the instruction to use rsi instead of rdi (change ModRM byte)
    auto load_b_inst = get_load_instruction(type_b);
    if (load_b_inst.size() >= 4) {
      load_b_inst[load_b_inst.size() - 1] = 0x06; // ModRM: [rsi]
      load_b_inst[load_b_inst.size() - 2] = 0x0f; // Use ymm1 instead of ymm0
    }
    emit_bytes(load_b_inst);

    // Add: vaddps ymm0, ymm0, ymm1
    emit_bytes({0xc5, 0xfc, 0x58, 0xc1});

    // Store: vmovups [rdx], ymm0
    emit_bytes({0xc5, 0xfc, 0x11, 0x02});

    // Return: ret
    emit_bytes({0xc3});

    // Flush instruction cache
    __builtin___clear_cache(start, code_ptr_);

    printf("Generated %zu bytes of code at %p\n", code_ptr_ - start, start);

    // Dump generated code (for debugging)
    printf("Generated code: ");
    for (uint8_t* p = start; p < code_ptr_; p++) {
      printf("%02x ", *p);
    }
    printf("\n\n");

    return reinterpret_cast<KernelFunc>(start);
  }

private:
  void emit_bytes(const std::vector<uint8_t>& bytes) {
    for (uint8_t b : bytes) {
      *code_ptr_++ = b;
    }
  }

  void* code_buffer_;
  uint8_t* code_ptr_;
};

// Wrapper that processes arrays in chunks
template <typename InputA, typename InputB>
void run_kernel(CodeGenerator::KernelFunc kernel, const InputA* a,
                const InputB* b, float* out, size_t n) {
  for (size_t i = 0; i + 8 <= n; i += 8) {
    kernel(a + i, b + i, out + i);
  }

  // Scalar fallback
  for (size_t i = (n / 8) * 8; i < n; i++) {
    out[i] = static_cast<float>(a[i]) + static_cast<float>(b[i]);
  }
}

int main() {
  printf("=== Runtime Code Generation Prototype ===\n\n");

  const size_t N = 16;

  // Test 1: Float32 + Float32
  {
    printf("Test 1: Float32 + Float32\n");
    printf("-----------------------------\n");

    std::vector<float> a(N), b(N), result(N, 0.0f);
    for (size_t i = 0; i < N; i++) {
      a[i] = static_cast<float>(i);
      b[i] = static_cast<float>(i * 2);
    }

    CodeGenerator gen;
    auto kernel = gen.generate_add_kernel(DataType::Float32, DataType::Float32);
    run_kernel(kernel, a.data(), b.data(), result.data(), N);

    printf("Results:\n");
    bool correct = true;
    for (size_t i = 0; i < 8; i++) { // Just show first 8
      float expected = a[i] + b[i];
      printf("  [%zu] %.1f + %.1f = %.1f (expected %.1f) %s\n", i, a[i], b[i],
             result[i], expected, result[i] == expected ? "✓" : "✗");
      if (result[i] != expected)
        correct = false;
    }
    printf("%s\n\n", correct ? "PASS" : "FAIL");
  }

  // Test 2: Int32 + Int32
  {
    printf("Test 2: Int32 + Int32\n");
    printf("-----------------------------\n");

    std::vector<int32_t> a(N), b(N);
    std::vector<float> result(N, 0.0f);
    for (size_t i = 0; i < N; i++) {
      a[i] = static_cast<int32_t>(i);
      b[i] = static_cast<int32_t>(i * 2);
    }

    CodeGenerator gen;
    auto kernel = gen.generate_add_kernel(DataType::Int32, DataType::Int32);
    run_kernel(kernel, a.data(), b.data(), result.data(), N);

    printf("Results:\n");
    bool correct = true;
    for (size_t i = 0; i < 8; i++) {
      float expected = static_cast<float>(a[i] + b[i]);
      printf("  [%zu] %d + %d = %.1f (expected %.1f) %s\n", i, a[i], b[i],
             result[i], expected, result[i] == expected ? "✓" : "✗");
      if (result[i] != expected)
        correct = false;
    }
    printf("%s\n\n", correct ? "PASS" : "FAIL");
  }

  // Test 3: Int32 + Float32 (mixed)
  {
    printf("Test 3: Int32 + Float32\n");
    printf("-----------------------------\n");

    std::vector<int32_t> a(N);
    std::vector<float> b(N);
    std::vector<float> result(N, 0.0f);
    for (size_t i = 0; i < N; i++) {
      a[i] = static_cast<int32_t>(i);
      b[i] = static_cast<float>(i * 2.5);
    }

    CodeGenerator gen;
    auto kernel = gen.generate_add_kernel(DataType::Int32, DataType::Float32);
    run_kernel(kernel, a.data(), b.data(), result.data(), N);

    printf("Results:\n");
    bool correct = true;
    for (size_t i = 0; i < 8; i++) {
      float expected = static_cast<float>(a[i]) + b[i];
      printf("  [%zu] %d + %.1f = %.1f (expected %.1f) %s\n", i, a[i], b[i],
             result[i], expected,
             fabs(result[i] - expected) < 0.01 ? "✓" : "✗");
      if (fabs(result[i] - expected) > 0.01)
        correct = false;
    }
    printf("%s\n\n", correct ? "PASS" : "FAIL");
  }

  printf("=== Code Generation Complete ===\n");
  return 0;
}
