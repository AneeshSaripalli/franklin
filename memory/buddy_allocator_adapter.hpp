#ifndef FRANKLIN_MEMORY_BUDDY_ALLOCATOR_ADAPTER_HPP
#define FRANKLIN_MEMORY_BUDDY_ALLOCATOR_ADAPTER_HPP

#include "memory/buddy_allocator.hpp"
#include <cstddef>
#include <memory>

namespace franklin {

// C++17 allocator adapter for buddy_allocator
// Allows buddy_allocator to be used with STL containers
//
// IMPORTANT: All allocations are rounded up to power-of-2 sizes
// with minimum 64-byte (cache line) granularity.
// This makes it safe to read entire AVX2/AVX512 registers without
// overrunning allocation boundaries.
//
// Example:
//   buddy_allocator pool(1024 * 1024);  // 1MB pool
//   buddy_allocator_adapter<int> alloc(&pool);
//   std::vector<int, buddy_allocator_adapter<int>> vec(alloc);
template <typename T> class buddy_allocator_adapter {
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
  using is_always_equal = std::false_type;

  // Rebind for different types
  template <typename U> struct rebind {
    using other = buddy_allocator_adapter<U>;
  };

  // Construct adapter with pointer to buddy_allocator
  // Note: Does NOT take ownership - caller must ensure allocator outlives
  // adapter
  explicit buddy_allocator_adapter(buddy_allocator* allocator) noexcept
      : allocator_(allocator) {}

  // Copy constructor (same type)
  buddy_allocator_adapter(const buddy_allocator_adapter& other) noexcept
      : allocator_(other.allocator_) {}

  // Copy constructor (different type, rebind)
  template <typename U>
  buddy_allocator_adapter(const buddy_allocator_adapter<U>& other) noexcept
      : allocator_(other.get_allocator()) {}

  // Move constructor
  buddy_allocator_adapter(buddy_allocator_adapter&& other) noexcept
      : allocator_(other.allocator_) {
    other.allocator_ = nullptr;
  }

  // Assignment
  buddy_allocator_adapter&
  operator=(const buddy_allocator_adapter& other) noexcept {
    allocator_ = other.allocator_;
    return *this;
  }

  // Move assignment
  buddy_allocator_adapter& operator=(buddy_allocator_adapter&& other) noexcept {
    allocator_ = other.allocator_;
    other.allocator_ = nullptr;
    return *this;
  }

  // Allocate n objects of type T
  // Returns pointer aligned to at least FRANKLIN_CACHE_LINE_SIZE
  [[nodiscard]] T* allocate(size_type n) {
    if (n == 0) {
      return nullptr;
    }

    // Calculate bytes needed
    size_type bytes = n * sizeof(T);

    // Allocate from buddy allocator
    // Note: buddy allocator rounds up to power-of-2, minimum 64 bytes
    void* ptr = allocator_->allocate(bytes);

    if (ptr == nullptr) {
      throw std::bad_alloc();
    }

    return static_cast<T*>(ptr);
  }

  // Deallocate previously allocated memory
  void deallocate(T* ptr, size_type n) noexcept {
    if (ptr == nullptr) {
      return;
    }

    allocator_->deallocate(ptr);
  }

  // Get the underlying buddy_allocator
  buddy_allocator* get_allocator() const noexcept { return allocator_; }

  // Equality comparison
  friend bool operator==(const buddy_allocator_adapter& lhs,
                         const buddy_allocator_adapter& rhs) noexcept {
    return lhs.allocator_ == rhs.allocator_;
  }

  friend bool operator!=(const buddy_allocator_adapter& lhs,
                         const buddy_allocator_adapter& rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  buddy_allocator* allocator_;

  // Allow rebind to access private members
  template <typename U> friend class buddy_allocator_adapter;
};

} // namespace franklin

#endif // FRANKLIN_MEMORY_BUDDY_ALLOCATOR_ADAPTER_HPP
