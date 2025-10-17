#ifndef FRANKLIN_MEMORY_ALIGNED_ALLOCATOR_HPP
#define FRANKLIN_MEMORY_ALIGNED_ALLOCATOR_HPP

#include "core/compiler_macros.hpp"
#include <cstddef>
#include <cstdlib>
#include <new>

namespace franklin {
namespace memory {

/// Cache-line aligned allocator for optimal memory access patterns
/// Modern x86-64 CPUs have 64-byte cache lines
template <typename T, std::size_t Alignment = FRANKLIN_CACHE_LINE_SIZE>
class aligned_allocator {
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::true_type;

  static constexpr std::size_t alignment = Alignment;

  constexpr aligned_allocator() noexcept = default;
  constexpr aligned_allocator(const aligned_allocator&) noexcept = default;

  template <typename U>
  constexpr aligned_allocator(const aligned_allocator<U, Alignment>&) noexcept {
  }

  [[nodiscard]] T* allocate(std::size_t n) {
    if (n == 0) {
      return nullptr;
    }

    // Check for overflow
    if (n > std::size_t(-1) / sizeof(T)) {
      throw std::bad_array_new_length();
    }

    std::size_t bytes = n * sizeof(T);

    // Align allocation size to cache line boundary for better performance
    // This ensures that each allocation starts on a cache line boundary
    void* ptr = nullptr;

#if defined(_MSC_VER)
    ptr = _aligned_malloc(bytes, Alignment);
    if (!ptr) {
      throw std::bad_alloc();
    }
#else
    // Use posix_memalign for Linux/Unix
    int result = posix_memalign(&ptr, Alignment, bytes);
    if (result != 0 || !ptr) {
      throw std::bad_alloc();
    }
#endif

    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, std::size_t) noexcept {
    if (ptr) {
#if defined(_MSC_VER)
      _aligned_free(ptr);
#else
      std::free(ptr);
#endif
    }
  }

  // Rebind for containers that need to allocate different types
  template <typename U> struct rebind {
    using other = aligned_allocator<U, Alignment>;
  };
};

template <typename T1, std::size_t A1, typename T2, std::size_t A2>
constexpr bool operator==(const aligned_allocator<T1, A1>&,
                          const aligned_allocator<T2, A2>&) noexcept {
  return A1 == A2;
}

template <typename T1, std::size_t A1, typename T2, std::size_t A2>
constexpr bool operator!=(const aligned_allocator<T1, A1>&,
                          const aligned_allocator<T2, A2>&) noexcept {
  return A1 != A2;
}

} // namespace memory
} // namespace franklin

#endif // FRANKLIN_MEMORY_ALIGNED_ALLOCATOR_HPP
