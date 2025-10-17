#ifndef FRANKLIN_CONTAINER_COLUMN_HPP
#define FRANKLIN_CONTAINER_COLUMN_HPP

#include "container/dynamic_bitset.hpp"
#include "core/bf16.hpp"
#include "core/compiler_macros.hpp"
#include "core/data_type_enum.hpp"
#include "memory/aligned_allocator.hpp"
#include <bit>
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
  requires std::is_same_v<decltype(T::policy_id), const DataTypeEnum::Enum>;
};

template <typename T>
concept PipelinePolicy = requires {
  typename T::value_type;
  typename T::storage_register_type;
  typename T::compute_register_type;
  { T::elements_per_iteration } -> std::convertible_to<std::size_t>;
};

} // namespace concepts

// Standard policy definitions - using cache-line aligned allocator
struct Int32DefaultPolicy {
  using value_type = std::int32_t;
  using allocator_type = memory::aligned_allocator<value_type, 64>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = true;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = true;
  static constexpr DataTypeEnum::Enum policy_id = DataTypeEnum::Int32Default;
};

struct Float32DefaultPolicy {
  using value_type = float;
  using allocator_type = memory::aligned_allocator<value_type, 64>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = true;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = true;
  static constexpr DataTypeEnum::Enum policy_id = DataTypeEnum::Float32Default;
};

struct BF16DefaultPolicy {
  using value_type = bf16;
  using allocator_type = memory::aligned_allocator<value_type, 64>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = true;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = true;
  static constexpr DataTypeEnum::Enum policy_id = DataTypeEnum::BF16Default;
};

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
  column_vector operator+(column_vector&& other) const;

  // Check if value at index is present (not missing/null)
  // Always performs bounds checking (returns false if out of bounds)
  // Safe for production use
  bool present(std::size_t index) const noexcept;

  // Unchecked version - only validates bounds in DEBUG builds
  // In optimized/production builds, no bounds checking is performed
  // Precondition: index < present_.size()
  // Use this in hot paths where you've already validated the index
  bool present_unchecked(std::size_t index) const noexcept;

  // Get runtime policy identifier for type erasure
  static constexpr DataTypeEnum::Enum policy() noexcept {
    return Policy::policy_id;
  }

  // Public accessors for data (needed for vectorization)
  const std::vector<value_type, allocator_type>& data() const noexcept {
    return data_;
  }
  std::vector<value_type, allocator_type>& data() noexcept { return data_; }
};

// Default constructor with optional allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(const allocator_type& alloc)
    : allocator_(alloc), data_(alloc), present_(alloc) {}

// Constructor with size and allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(std::size_t size,
                                     const allocator_type& alloc)
    : allocator_(alloc) {
  // Round up to cache-line boundary for proper SIMD alignment
  // 64-byte cache line / sizeof(value_type) elements per cache line
  constexpr std::size_t elements_per_cache_line = 64 / sizeof(value_type);
  std::size_t rounded_size =
      ((size + elements_per_cache_line - 1) / elements_per_cache_line) *
      elements_per_cache_line;

  data_ = std::vector<value_type, allocator_type>(rounded_size, alloc);
  present_ = std::vector<bool, bool_allocator_type>(rounded_size, true,
                                                    bool_allocator_type(alloc));
  // Mark only the requested elements as present
  for (std::size_t i = size; i < rounded_size; ++i) {
    present_[i] = false;
  }
}

// Constructor with size, value, and allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(std::size_t size, const value_type& value,
                                     const allocator_type& alloc)
    : allocator_(alloc) {
  // Round up to cache-line boundary for proper SIMD alignment
  // 64-byte cache line / sizeof(value_type) elements per cache line
  constexpr std::size_t elements_per_cache_line = 64 / sizeof(value_type);
  std::size_t rounded_size =
      ((size + elements_per_cache_line - 1) / elements_per_cache_line) *
      elements_per_cache_line;

  data_ = std::vector<value_type, allocator_type>(rounded_size, value, alloc);
  present_ = std::vector<bool, bool_allocator_type>(rounded_size, true,
                                                    bool_allocator_type(alloc));
  // Mark only the requested elements as present
  for (std::size_t i = size; i < rounded_size; ++i) {
    present_[i] = false;
  }
}

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

// Default pipeline for int32_t - no transformation needed
struct Int32Pipeline {
  using value_type = std::int32_t;
  using storage_register_type = __m256i;
  using compute_register_type = __m256i;
  static constexpr std::size_t elements_per_iteration = 8;

  FRANKLIN_FORCE_INLINE static storage_register_type
  load(const value_type* ptr) {
    return _mm256_load_si256(reinterpret_cast<const __m256i*>(ptr));
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  transform_to(storage_register_type reg) {
    return reg; // no-op for primitives
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  op(compute_register_type a, compute_register_type b) {
    return _mm256_add_epi32(a, b);
  }

  FRANKLIN_FORCE_INLINE static storage_register_type
  transform_from(compute_register_type reg) {
    return reg; // no-op for primitives
  }

  FRANKLIN_FORCE_INLINE static void store(value_type* ptr,
                                          storage_register_type reg) {
    _mm256_store_si256(reinterpret_cast<__m256i*>(ptr), reg);
  }
};

// BF16 pipeline - converts to fp32 for computation using native instructions
struct BF16Pipeline {
  using value_type = bf16;
  using storage_register_type = __m128i; // 8 bf16 values = 16 bytes
  using compute_register_type = __m256;  // 8 fp32 values = 32 bytes
  static constexpr std::size_t elements_per_iteration = 8;

  FRANKLIN_FORCE_INLINE static storage_register_type
  load(const value_type* ptr) {
    return _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
  }

  // Convert 8 bf16 to 8 fp32 using native AVX-512 BF16 instruction
  FRANKLIN_FORCE_INLINE static compute_register_type
  transform_to(storage_register_type bf16_reg) {
    // _mm256_cvtpbh_ps: Convert packed BF16 to packed single-precision FP
    // Takes __m128bh (8 bf16) and returns __m256 (8 fp32)
    return _mm256_cvtpbh_ps(reinterpret_cast<__m128bh>(bf16_reg));
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  op(compute_register_type a, compute_register_type b) {
    return _mm256_add_ps(a, b);
  }

  // Convert 8 fp32 back to 8 bf16 using native AVX-512 BF16 instruction
  // This provides proper rounding instead of truncation
  FRANKLIN_FORCE_INLINE static storage_register_type
  transform_from(compute_register_type fp32_reg) {
    // _mm256_cvtneps_pbh: Convert packed single-precision FP to packed BF16
    // Takes __m256 (8 fp32) and returns __m128bh (8 bf16) with rounding
    __m128bh result = _mm256_cvtneps_pbh(fp32_reg);
    return reinterpret_cast<__m128i>(result);
  }

  FRANKLIN_FORCE_INLINE static void store(value_type* ptr,
                                          storage_register_type reg) {
    _mm_store_si128(reinterpret_cast<__m128i*>(ptr), reg);
  }
};

template <concepts::ColumnPolicy ColPolicy, concepts::PipelinePolicy Pipeline>
static void vectorize(column_vector<ColPolicy> const& fst,
                      column_vector<ColPolicy> const& snd,
                      column_vector<ColPolicy>& out) {

  using value_type = typename ColPolicy::value_type;
  const value_type* fst_ptr = fst.data().data();
  const value_type* snd_ptr = snd.data().data();
  value_type* out_ptr = out.data().data();

  std::size_t offset = 0;
  const std::size_t step = Pipeline::elements_per_iteration;
  const std::size_t num_elements = out.data().size();

  while (offset + step <= num_elements) {
    // Load from memory
    auto a_storage = Pipeline::load(fst_ptr + offset);
    auto b_storage = Pipeline::load(snd_ptr + offset);

    // Transform to compute domain (e.g., bf16 -> fp32)
    auto a_compute = Pipeline::transform_to(a_storage);
    auto b_compute = Pipeline::transform_to(b_storage);

    // Perform operation in compute domain
    auto result_compute = Pipeline::op(a_compute, b_compute);

    // Transform back to storage domain (e.g., bf16 -> fp32)
    auto result_storage = Pipeline::transform_from(result_compute);

    // Store to memory
    Pipeline::store(out_ptr + offset, result_storage);

    offset += step;
  }

  // Note: No tail handling needed - buffers are cache-line aligned by design.
  // The present_ bitmask handles validity of elements within the padded buffer.
}

template <concepts::ColumnPolicy ColPolicy, concepts::PipelinePolicy Pipeline>
static void vectorize_destructive(column_vector<ColPolicy>& mut,
                                  column_vector<ColPolicy> const& snd) {

  using value_type = typename ColPolicy::value_type;
  value_type* mut_ptr = mut.data().data();
  const value_type* snd_ptr = snd.data().data();

  std::size_t offset = 0;
  const std::size_t step = Pipeline::elements_per_iteration;
  const std::size_t num_elements =
      std::min(mut.data().size(), snd.data().size());

  while (offset + step <= num_elements) {
    // Load from both sources
    auto a_storage = Pipeline::load(mut_ptr + offset);
    auto b_storage = Pipeline::load(snd_ptr + offset);

    // Transform to compute domain (e.g., bf16 -> fp32)
    auto a_compute = Pipeline::transform_to(a_storage);
    auto b_compute = Pipeline::transform_to(b_storage);

    // Perform operation in compute domain
    auto result_compute = Pipeline::op(a_compute, b_compute);

    // Transform back to storage domain (e.g., fp32 -> bf16)
    auto result_storage = Pipeline::transform_from(result_compute);

    // Store back to mut (reusing its buffer)
    Pipeline::store(mut_ptr + offset, result_storage);

    offset += step;
  }

  // Note: No tail handling needed - buffers are cache-line aligned by design.
  // The present_ bitmask handles validity of elements within the padded buffer.
}

// Element-wise addition
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator+(const column_vector& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, Int32Pipeline>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, BF16Pipeline>(*this, other, output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator+(column_vector&& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    vectorize_destructive<Policy, Int32Pipeline>(other, *this);
    return other;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    vectorize_destructive<Policy, BF16Pipeline>(other, *this);
    return other;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

template <concepts::ColumnPolicy Policy>
bool column_vector<Policy>::present(std::size_t index) const noexcept {
  if (index >= present_.size()) [[unlikely]] {
    return false;
  }
  return present_[index];
}

template <concepts::ColumnPolicy Policy>
bool column_vector<Policy>::present_unchecked(
    std::size_t index) const noexcept {
  FRANKLIN_DEBUG_ASSERT(index < present_.size() &&
                        "present() index out of bounds");
  return present_[index];
}

} // namespace franklin

#endif // FRANKLIN_CONTAINER_COLUMN_HPP
