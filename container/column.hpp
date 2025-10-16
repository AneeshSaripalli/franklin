#include "container/dynamic_bitset.hpp"
#include "core/compiler_macros.hpp"
#include <concepts>
#include <cstdint>
#include <immintrin.h>
#include <memory>
#include <vector>

namespace franklin {

namespace concepts {

template <typename T>
concept ColumnPolicy = requires {
  typename T::value_type;
  typename T::allocator_type;
  requires std::is_same_v<decltype(T::is_view), const bool>;
  requires std::is_same_v<decltype(T::allow_missing), const bool>;
  requires std::is_same_v<decltype(T::use_avx512), const bool>;
  requires std::is_same_v<decltype(T::assume_aligned), const bool>;
};

} // namespace concepts

template <concepts::ColumnPolicy Policy> class column_vector {
public:
  using value_type = typename Policy::value_type;
  using allocator_type = typename Policy::allocator_type;
  using bool_allocator_type = typename std::allocator_traits<
      allocator_type>::template rebind_alloc<bool>;

  static constexpr bool is_view = Policy::is_view;
  static constexpr bool allow_missing = Policy::allow_missing;
  static constexpr bool use_avx512 = Policy::use_avx512;
  static constexpr bool assume_aligned = Policy::assume_aligned;

private:
  allocator_type allocator_;
  // These can be compressed if the data has extra bits in its representation
  // that we can use. E.g. pointers are a notable example.
  // Also stores the size of the data twice, which is dumb. 8 bytes per column!.
  std::vector<value_type, allocator_type> data_;
  std::vector<bool, bool_allocator_type> present_;

public:
  // Default constructor with optional allocator
  explicit column_vector(const allocator_type& alloc = allocator_type());

  // Constructor with size and allocator
  explicit column_vector(std::size_t size,
                         const allocator_type& alloc = allocator_type());

  // Constructor with size, value, and allocator
  column_vector(std::size_t size, const value_type& value,
                const allocator_type& alloc = allocator_type());

  // Copy constructor with allocator
  column_vector(const column_vector& other,
                const allocator_type& alloc = allocator_type());

  // Move constructor
  column_vector(column_vector&& other) noexcept;

  // Copy assignment
  column_vector& operator=(const column_vector& other) noexcept;

  // Move assignment
  column_vector& operator=(column_vector&& other) noexcept;

  // Element-wise operations
  column_vector operator+(const column_vector& other) const;
  column_vector operator-(const column_vector& other) const;
  column_vector operator*(const column_vector& other) const;
  column_vector operator/(const column_vector& other) const;
  column_vector operator^(const column_vector& other) const;
  column_vector operator&(const column_vector& other) const;
  column_vector operator%(const column_vector& other) const;
  column_vector operator|(const column_vector& other) const;

  // Check if value at index is present (not missing/null)
  // Performs bounds checking in debug builds
  bool present(std::size_t index) const noexcept;

  // Unchecked version - no bounds checking, faster
  // Precondition: index < size()
  bool present_unchecked(std::size_t index) const noexcept;
};

// Default constructor with optional allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(const allocator_type& alloc)
    : allocator_(alloc), data_(alloc), present_(alloc) {}

// Constructor with size and allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(std::size_t size,
                                     const allocator_type& alloc)
    : allocator_(alloc), data_(size, alloc), present_(size, alloc) {}

// Constructor with size, value, and allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(std::size_t size, const value_type& value,
                                     const allocator_type& alloc)
    : allocator_(alloc), data_(size, value, alloc),
      present_(size, true, alloc) {}

// Copy constructor with allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(const column_vector& other,
                                     const allocator_type& alloc)
    : allocator_(alloc), data_(other.data_, alloc),
      present_(other.present_, alloc) {}

// Move constructor
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(column_vector&& other) noexcept
    : allocator_(std::move(other.allocator_)), data_(std::move(other.data_)),
      present_(std::move(other.present_)) {}

// Copy assignment
template <concepts::ColumnPolicy Policy>
column_vector<Policy>&
column_vector<Policy>::operator=(const column_vector& other) noexcept {
  if (this != &other) {
    data_ = other.data_;
    present_ = other.present_;
    // Note: allocator is not copied per C++ allocator semantics
  }
  return *this;
}

// Move assignment
template <concepts::ColumnPolicy Policy>
column_vector<Policy>&
column_vector<Policy>::operator=(column_vector&& other) noexcept {
  if (this != &other) {
    data_ = std::move(other.data_);
    present_ = std::move(other.present_);
    allocator_ = std::move(other.allocator_);
  }
  return *this;
}

template <concepts::ColumnPolicy Policy, typename SIMDLoad, typename SIMDStore,
          typename SIMDOp, typename Allocator>
static void vectorize(column_vector<Policy> const& fst,
                      column_vector<Policy> const& snd,
                      column_vector<Policy>& out) {

  using register_type = __m256i;

  register_type* data_pointer =
      std::bit_cast<const register_type>(fst.data_.data());
  register_type* other_pointer =
      std::bit_cast<const register_type>(snd.data_.data());
  register_type* store_pointer =
      std::bit_cast<const register_type>(out.data_.data());

  static constexpr auto simd_load = _mm256_load_si256;
  static constexpr auto simd_op = _mm256_add_epi32;
  static constexpr auto simd_store = _mm256_store_si256;

  auto offset = 0UL;

  while (true) {
    auto a1 = simd_load(data_pointer + offset);
    auto a2 = simd_load(other_pointer + offset);

    auto b1 = simd_load(data_pointer + offset + 1);
    auto b2 = simd_load(other_pointer + offset + 1);

    auto c1 = simd_op(a1, b1);
    auto c2 = simd_op(a2, b2);

    // Store back to new array
    simd_store(store_pointer + offset, c1);
    simd_store(store_pointer + offset + 1, c2);

    if (offset >= out.data_.size()) {
      break;
    }
  }
}

template <concepts::ColumnPolicy Policy, typename SIMDLoad, typename SIMDStore,
          typename SIMDOp, typename Allocator>
static void vectorize_destructive(column_vector<Policy>& mut,
                                  column_vector<Policy> const& snd) {

  using register_type = __m256i;

  register_type* load_store_pointer =
      std::bit_cast<const register_type>(mut.data_.data());
  register_type* other_pointer =
      std::bit_cast<const register_type>(snd.data_.data());

  static constexpr auto simd_load = _mm256_load_si256;
  static constexpr auto simd_op = _mm256_add_epi32;
  static constexpr auto simd_store = _mm256_store_si256;

  auto offset = 0UL;

  while (true) {
    auto a1 = simd_load(load_store_pointer + offset);
    auto a2 = simd_load(other_pointer + offset);

    auto b1 = simd_load(load_store_pointer + offset + 1);
    auto b2 = simd_load(other_pointer + offset + 1);

    auto c1 = simd_op(a1, b1);
    auto c2 = simd_op(a2, b2);

    // Store back to new array
    simd_store(load_store_pointer + offset, c1);
    simd_store(load_store_pointer + offset + 1, c2);

    if (offset >= mut.data_.size()) {
      break;
    }
  }
}

// Element-wise addition
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator+(const column_vector& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    using register_type = __m256i;

    static constexpr auto simd_load = _mm256_load_si256;
    static constexpr auto simd_op = _mm256_add_epi32;
    static constexpr auto simd_store = _mm256_store_si256;

    vectorize<Policy, simd_load, simd_store, simd_op, allocator_type>(
        *this, other, &output);
    return output;
  } else if (std::is_same_v<value_type, float>) {

  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

// Element-wise subtraction
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator-(const column_vector& other) const {
  // TODO: Implement element-wise subtraction
  return column_vector{allocator_};
}

// Element-wise multiplication
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator*(const column_vector& other) const {
  // TODO: Implement element-wise multiplication
  return column_vector{allocator_};
}

// Element-wise division
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator/(const column_vector& other) const {
  // TODO: Implement element-wise division
  return column_vector{allocator_};
}

// Element-wise XOR
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator^(const column_vector& other) const {
  // TODO: Implement element-wise XOR
  return column_vector{allocator_};
}

// Element-wise bitwise AND
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator&(const column_vector& other) const {
  // TODO: Implement element-wise bitwise AND
  return column_vector{allocator_};
}

// Element-wise modulo
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator%(const column_vector& other) const {
  // TODO: Implement element-wise modulo
  return column_vector{allocator_};
}

// Element-wise bitwise OR
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator|(const column_vector& other) const {
  // TODO: Implement element-wise bitwise OR
  return column_vector{allocator_};
}

template <concepts::ColumnPolicy Policy>
bool column_vector<Policy>::present(std::size_t index) const noexcept {
  FRANKLIN_DEBUG_ASSERT(index < present_.size() &&
                        "present() index out of bounds");
  return present_[index];
}

template <concepts::ColumnPolicy Policy>
bool column_vector<Policy>::present_unchecked(
    std::size_t index) const noexcept {
  return present_[index];
}

} // namespace franklin