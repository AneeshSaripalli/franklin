// Check actual AVX2 instruction encodings
#include <immintrin.h>

__attribute__((noinline)) __m256 load_float(const float* ptr) {
  return _mm256_loadu_ps(ptr);
}

__attribute__((noinline)) __m256 load_int32_cvt(const int32_t* ptr) {
  return _mm256_cvtepi32_ps(
      _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr)));
}

__attribute__((noinline)) void add_and_store(const float* a, const float* b,
                                             float* out) {
  __m256 va = _mm256_loadu_ps(a);
  __m256 vb = _mm256_loadu_ps(b);
  __m256 result = _mm256_add_ps(va, vb);
  _mm256_storeu_ps(out, result);
}

int main() {
  float a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  float b[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  float c[8];

  add_and_store(a, b, c);

  return 0;
}
