// Working runtime code generation for type-specialized kernels
//
// Uses actual instruction bytes from objdump to generate correct AVX2 code.
//
// Build: g++ -std=c++20 -O2 -mavx2 -o codegen runtime_codegen_working.cpp
// Run: ./codegen

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

enum class DataType { Int32, Float32 };

// Actual AVX2 instruction bytes (from objdump)
namespace Inst {
// vmovups ymm0, [rdi]
constexpr uint8_t LOAD_FP32_YMM0_RDI[] = {0xc5, 0xfc, 0x10, 0x07};

// vcvtdq2ps ymm0, [rdi]  (load int32, convert to float)
constexpr uint8_t LOAD_INT32_CVT_YMM0_RDI[] = {0xc5, 0xfc, 0x5b, 0x07};

// vmovups ymm1, [rsi]
constexpr uint8_t LOAD_FP32_YMM1_RSI[] = {0xc5, 0xfc, 0x10, 0x0e};

// vcvtdq2ps ymm1, [rsi]
constexpr uint8_t LOAD_INT32_CVT_YMM1_RSI[] = {0xc5, 0xfc, 0x5b, 0x0e};

// vaddps ymm0, ymm0, ymm1
constexpr uint8_t ADD_YMM0_YMM1[] = {0xc5, 0xfc, 0x58, 0xc1};

// vmovups [rdx], ymm0
constexpr uint8_t STORE_YMM0_RDX[] = {0xc5, 0xfc, 0x11, 0x02};

// ret
constexpr uint8_t RET[] = {0xc3};
} // namespace Inst

// Simple executable code buffer
class ExecutableBuffer {
public:
  ExecutableBuffer() {
    size_t page_size = sysconf(_SC_PAGESIZE);
    buffer_ = mmap(nullptr, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (buffer_ == MAP_FAILED) {
      perror("mmap failed");
      buffer_ = nullptr;
      return;
    }

    write_ptr_ = static_cast<uint8_t*>(buffer_);
    printf("Allocated executable buffer at %p\n", buffer_);
  }

  ~ExecutableBuffer() {
    if (buffer_) {
      munmap(buffer_, sysconf(_SC_PAGESIZE));
    }
  }

  template <size_t N> void emit(const uint8_t (&bytes)[N]) {
    memcpy(write_ptr_, bytes, N);
    write_ptr_ += N;
  }

  void* get_function() {
    // Flush instruction cache
    __builtin___clear_cache(buffer_, write_ptr_);
    return buffer_;
  }

  void dump() {
    printf("Generated code (%zu bytes): ",
           write_ptr_ - static_cast<uint8_t*>(buffer_));
    for (uint8_t* p = static_cast<uint8_t*>(buffer_); p < write_ptr_; p++) {
      printf("%02x ", *p);
    }
    printf("\n");
  }

private:
  void* buffer_;
  uint8_t* write_ptr_;
};

// Generate specialized add kernel
// Signature: void kernel(const void* a, const void* b, float* out)
// System V AMD64 ABI: rdi=a, rsi=b, rdx=out
using AddKernel = void (*)(const void*, const void*, float*);

// Global buffer pool (simple version - never freed)
std::vector<ExecutableBuffer*> kernel_buffers;

AddKernel generate_add_kernel(DataType type_a, DataType type_b) {
  printf("\nGenerating kernel for: %s + %s -> Float32\n",
         type_a == DataType::Float32 ? "Float32" : "Int32",
         type_b == DataType::Float32 ? "Float32" : "Int32");

  // Allocate persistent buffer
  auto* buf = new ExecutableBuffer();
  kernel_buffers.push_back(buf);

  // Load input A into ymm0
  if (type_a == DataType::Float32) {
    buf->emit(Inst::LOAD_FP32_YMM0_RDI);
  } else {
    buf->emit(Inst::LOAD_INT32_CVT_YMM0_RDI);
  }

  // Load input B into ymm1
  if (type_b == DataType::Float32) {
    buf->emit(Inst::LOAD_FP32_YMM1_RSI);
  } else {
    buf->emit(Inst::LOAD_INT32_CVT_YMM1_RSI);
  }

  // Add: ymm0 = ymm0 + ymm1
  buf->emit(Inst::ADD_YMM0_YMM1);

  // Store result
  buf->emit(Inst::STORE_YMM0_RDX);

  // Return
  buf->emit(Inst::RET);

  buf->dump();

  return reinterpret_cast<AddKernel>(buf->get_function());
}

// Wrapper to process arrays
template <typename A, typename B>
void add_arrays(AddKernel kernel, const A* a, const B* b, float* out,
                size_t n) {
  // Vectorized portion
  for (size_t i = 0; i + 8 <= n; i += 8) {
    kernel(a + i, b + i, out + i);
  }

  // Scalar tail
  for (size_t i = (n / 8) * 8; i < n; i++) {
    out[i] = static_cast<float>(a[i]) + static_cast<float>(b[i]);
  }
}

void test_kernel(const char* name, DataType type_a, DataType type_b,
                 const void* a, const void* b, float* expected, size_t n) {
  printf("\n========================================\n");
  printf("Test: %s\n", name);
  printf("========================================\n");

  std::vector<float> result(n, 0.0f);

  // Generate specialized kernel
  AddKernel kernel = generate_add_kernel(type_a, type_b);

  // Process first chunk manually to show it works
  kernel(a, b, result.data());

  // Check results
  printf("\nResults (first 8 elements):\n");
  bool correct = true;
  for (size_t i = 0; i < 8; i++) {
    bool match = fabs(result[i] - expected[i]) < 0.01f;
    printf("  [%zu] %.2f (expected %.2f) %s\n", i, result[i], expected[i],
           match ? "✓" : "✗");
    if (!match)
      correct = false;
  }

  printf("\n%s\n", correct ? "PASS ✓" : "FAIL ✗");
}

int main() {
  printf("==============================================\n");
  printf("Runtime Code Generation - Type Specialization\n");
  printf("==============================================\n");

  const size_t N = 16;

  // Test 1: Float32 + Float32
  {
    std::vector<float> a(N), b(N), expected(N);
    for (size_t i = 0; i < N; i++) {
      a[i] = static_cast<float>(i);
      b[i] = static_cast<float>(i * 2);
      expected[i] = a[i] + b[i];
    }

    test_kernel("Float32 + Float32", DataType::Float32, DataType::Float32,
                a.data(), b.data(), expected.data(), N);
  }

  // Test 2: Int32 + Int32
  {
    std::vector<int32_t> a(N), b(N);
    std::vector<float> expected(N);
    for (size_t i = 0; i < N; i++) {
      a[i] = static_cast<int32_t>(i);
      b[i] = static_cast<int32_t>(i * 3);
      expected[i] = static_cast<float>(a[i] + b[i]);
    }

    test_kernel("Int32 + Int32", DataType::Int32, DataType::Int32, a.data(),
                b.data(), expected.data(), N);
  }

  // Test 3: Int32 + Float32 (mixed types)
  {
    std::vector<int32_t> a(N);
    std::vector<float> b(N), expected(N);
    for (size_t i = 0; i < N; i++) {
      a[i] = static_cast<int32_t>(i * 10);
      b[i] = static_cast<float>(i * 0.5);
      expected[i] = static_cast<float>(a[i]) + b[i];
    }

    test_kernel("Int32 + Float32", DataType::Int32, DataType::Float32, a.data(),
                b.data(), expected.data(), N);
  }

  // Test 4: Float32 + Int32 (reverse mixed)
  {
    std::vector<float> a(N);
    std::vector<int32_t> b(N);
    std::vector<float> expected(N);
    for (size_t i = 0; i < N; i++) {
      a[i] = static_cast<float>(i * 1.5);
      b[i] = static_cast<int32_t>(i * 100);
      expected[i] = a[i] + static_cast<float>(b[i]);
    }

    test_kernel("Float32 + Int32", DataType::Float32, DataType::Int32, a.data(),
                b.data(), expected.data(), N);
  }

  printf("\n==============================================\n");
  printf("All tests complete!\n");
  printf("==============================================\n");

  return 0;
}
