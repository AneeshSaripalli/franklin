#ifndef FRANKLIN_MATH_UTILS_HPP
#define FRANKLIN_MATH_UTILS_HPP

#include <concepts>

namespace franklin {

template <std::unsigned_integral T>
static constexpr bool is_pow2(T x) noexcept {
  return x && (x & (x - 1)) == 0;
}

template <std::unsigned_integral T>
static constexpr T round_pow2(T x) noexcept {
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  if constexpr (sizeof(x) > 1)
    x |= x >> 8;
  if constexpr (sizeof(x) > 2)
    x |= x >> 16;
  if constexpr (sizeof(x) > 4)
    x |= x >> 32;
  return ++x;
}

template <std::unsigned_integral T> static constexpr T next_pow2(T x) noexcept {
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  if constexpr (sizeof(x) > 1)
    x |= x >> 8;
  if constexpr (sizeof(x) > 2)
    x |= x >> 16;
  if constexpr (sizeof(x) > 4)
    x |= x >> 32;
  return ++x;
}

} // namespace franklin

#endif // FRANKLIN_MATH_UTILS_HPP
