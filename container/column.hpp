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

// Policy for dynamic_bitset
struct BitsetPolicy {
  using size_type = std::size_t;
};

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
  dynamic_bitset<BitsetPolicy> present_mask_;

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

  column_vector operator-(const column_vector& other) const;
  column_vector operator-(column_vector&& other) const;

  column_vector operator*(const column_vector& other) const;
  column_vector operator*(column_vector&& other) const;

  // Scalar operations (column op scalar)
  column_vector operator+(value_type scalar) const;
  column_vector operator-(value_type scalar) const;
  column_vector operator*(value_type scalar) const;

  // Friend operators for (scalar op column)
  friend column_vector operator+(value_type scalar, const column_vector& col) {
    return col + scalar; // Addition is commutative
  }
  friend column_vector operator*(value_type scalar, const column_vector& col) {
    return col * scalar; // Multiplication is commutative
  }

  // Forward declaration needed for friend template
  template <concepts::ColumnPolicy P>
  friend column_vector<P> operator-(typename P::value_type scalar,
                                    const column_vector<P>& col);

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

  // Public accessors for present bitmask (needed for vectorization)
  const dynamic_bitset<BitsetPolicy>& present_mask() const noexcept {
    return present_mask_;
  }
  dynamic_bitset<BitsetPolicy>& present_mask() noexcept {
    return present_mask_;
  }
};

// Default constructor with optional allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(const allocator_type& alloc)
    : allocator_(alloc), data_(alloc), present_mask_() {}

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
  present_mask_ = dynamic_bitset<BitsetPolicy>(rounded_size, true);
  // Mark only the requested elements as present
  for (std::size_t i = size; i < rounded_size; ++i) {
    present_mask_.set(i, false);
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
  present_mask_ = dynamic_bitset<BitsetPolicy>(rounded_size, true);
  // Mark only the requested elements as present
  for (std::size_t i = size; i < rounded_size; ++i) {
    present_mask_.set(i, false);
  }
}

// Copy constructor with allocator
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(const column_vector& other,
                                     const allocator_type& alloc)
    : allocator_(alloc), data_(other.data_, alloc),
      present_mask_(other.present_mask_) {}

// Move constructor
template <concepts::ColumnPolicy Policy>
column_vector<Policy>::column_vector(column_vector&& other) noexcept
    : allocator_(std::move(other.allocator_)), data_(std::move(other.data_)),
      present_mask_(std::move(other.present_mask_)) {}

// Copy assignment
template <concepts::ColumnPolicy Policy>
column_vector<Policy>&
column_vector<Policy>::operator=(const column_vector& other) noexcept {
  if (this != &other) {
    data_ = other.data_;
    present_mask_ = other.present_mask_;
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
    present_mask_ = std::move(other.present_mask_);
    allocator_ = std::move(other.allocator_);
  }
  return *this;
}

// Operation types for vectorized computation
enum class OpType { Add, Sub, Mul };

// Default pipeline for int32_t - no transformation needed
template <OpType Op> struct Int32Pipeline {
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
    if constexpr (Op == OpType::Add) {
      return _mm256_add_epi32(a, b);
    } else if constexpr (Op == OpType::Sub) {
      return _mm256_sub_epi32(a, b);
    } else if constexpr (Op == OpType::Mul) {
      return _mm256_mullo_epi32(a, b);
    }
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

// Float32 pipeline - direct AVX2 operations on float
template <OpType Op> struct Float32Pipeline {
  using value_type = float;
  using storage_register_type = __m256;
  using compute_register_type = __m256;
  static constexpr std::size_t elements_per_iteration = 8;

  FRANKLIN_FORCE_INLINE static storage_register_type
  load(const value_type* ptr) {
    return _mm256_load_ps(ptr);
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  transform_to(storage_register_type reg) {
    return reg; // no-op for primitives
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  op(compute_register_type a, compute_register_type b) {
    if constexpr (Op == OpType::Add) {
      return _mm256_add_ps(a, b);
    } else if constexpr (Op == OpType::Sub) {
      return _mm256_sub_ps(a, b);
    } else if constexpr (Op == OpType::Mul) {
      return _mm256_mul_ps(a, b);
    }
  }

  FRANKLIN_FORCE_INLINE static storage_register_type
  transform_from(compute_register_type reg) {
    return reg; // no-op for primitives
  }

  FRANKLIN_FORCE_INLINE static void store(value_type* ptr,
                                          storage_register_type reg) {
    _mm256_store_ps(ptr, reg);
  }
};

// BF16 pipeline - converts to fp32 for computation using native instructions
template <OpType Op> struct BF16Pipeline {
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
    if constexpr (Op == OpType::Add) {
      return _mm256_add_ps(a, b);
    } else if constexpr (Op == OpType::Sub) {
      return _mm256_sub_ps(a, b);
    } else if constexpr (Op == OpType::Mul) {
      return _mm256_mul_ps(a, b);
    }
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

// Scalar broadcast helpers for each pipeline type
template <OpType Op> struct Int32ScalarPipeline {
  using value_type = std::int32_t;
  using storage_register_type = __m256i;
  using compute_register_type = __m256i;
  static constexpr std::size_t elements_per_iteration = 8;

  FRANKLIN_FORCE_INLINE static compute_register_type
  broadcast(value_type scalar) {
    return _mm256_set1_epi32(scalar);
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  op(compute_register_type a, compute_register_type b) {
    if constexpr (Op == OpType::Add) {
      return _mm256_add_epi32(a, b);
    } else if constexpr (Op == OpType::Sub) {
      return _mm256_sub_epi32(a, b);
    } else if constexpr (Op == OpType::Mul) {
      return _mm256_mullo_epi32(a, b);
    }
  }

  FRANKLIN_FORCE_INLINE static storage_register_type
  load(const value_type* ptr) {
    return _mm256_load_si256(reinterpret_cast<const __m256i*>(ptr));
  }

  FRANKLIN_FORCE_INLINE static void store(value_type* ptr,
                                          storage_register_type reg) {
    _mm256_store_si256(reinterpret_cast<__m256i*>(ptr), reg);
  }
};

template <OpType Op> struct Float32ScalarPipeline {
  using value_type = float;
  using storage_register_type = __m256;
  using compute_register_type = __m256;
  static constexpr std::size_t elements_per_iteration = 8;

  FRANKLIN_FORCE_INLINE static compute_register_type
  broadcast(value_type scalar) {
    return _mm256_set1_ps(scalar);
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  op(compute_register_type a, compute_register_type b) {
    if constexpr (Op == OpType::Add) {
      return _mm256_add_ps(a, b);
    } else if constexpr (Op == OpType::Sub) {
      return _mm256_sub_ps(a, b);
    } else if constexpr (Op == OpType::Mul) {
      return _mm256_mul_ps(a, b);
    }
  }

  FRANKLIN_FORCE_INLINE static storage_register_type
  load(const value_type* ptr) {
    return _mm256_load_ps(ptr);
  }

  FRANKLIN_FORCE_INLINE static void store(value_type* ptr,
                                          storage_register_type reg) {
    _mm256_store_ps(ptr, reg);
  }
};

template <OpType Op> struct BF16ScalarPipeline {
  using value_type = bf16;
  using storage_register_type = __m128i;
  using compute_register_type = __m256;
  static constexpr std::size_t elements_per_iteration = 8;

  FRANKLIN_FORCE_INLINE static compute_register_type
  broadcast(value_type scalar) {
    // Convert bf16 scalar to fp32 and broadcast
    float fp32_scalar = scalar.to_float();
    return _mm256_set1_ps(fp32_scalar);
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  op(compute_register_type a, compute_register_type b) {
    if constexpr (Op == OpType::Add) {
      return _mm256_add_ps(a, b);
    } else if constexpr (Op == OpType::Sub) {
      return _mm256_sub_ps(a, b);
    } else if constexpr (Op == OpType::Mul) {
      return _mm256_mul_ps(a, b);
    }
  }

  FRANKLIN_FORCE_INLINE static storage_register_type
  load(const value_type* ptr) {
    return _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
  }

  FRANKLIN_FORCE_INLINE static compute_register_type
  transform_to(storage_register_type bf16_reg) {
    return _mm256_cvtpbh_ps(reinterpret_cast<__m128bh>(bf16_reg));
  }

  FRANKLIN_FORCE_INLINE static storage_register_type
  transform_from(compute_register_type fp32_reg) {
    __m128bh result = _mm256_cvtneps_pbh(fp32_reg);
    return reinterpret_cast<__m128i>(result);
  }

  FRANKLIN_FORCE_INLINE static void store(value_type* ptr,
                                          storage_register_type reg) {
    _mm_store_si128(reinterpret_cast<__m128i*>(ptr), reg);
  }
};

// AVX2-optimized bitset AND: dst &= src
// Note: dynamic_bitset's operator&= uses AVX2 instructions internally
// via its optimized block-level operations (see dynamic_bitset.hpp:307-325)
inline void bitset_and_avx2(dynamic_bitset<BitsetPolicy>& dst,
                            const dynamic_bitset<BitsetPolicy>& src) {
  dst &= src;
}

// Vectorized scalar operation: column op scalar
template <concepts::ColumnPolicy ColPolicy, typename ScalarPipeline>
static void vectorize_scalar(column_vector<ColPolicy> const& input,
                             typename ColPolicy::value_type scalar,
                             column_vector<ColPolicy>& output) {
  using value_type = typename ColPolicy::value_type;
  const value_type* input_ptr = input.data().data();
  value_type* output_ptr = output.data().data();

  std::size_t offset = 0;
  const std::size_t step = ScalarPipeline::elements_per_iteration;
  const std::size_t num_elements = output.data().size();

  // Broadcast scalar to all lanes
  auto scalar_reg = ScalarPipeline::broadcast(scalar);

  while (offset + step <= num_elements) {
    // Load column data
    auto input_storage = ScalarPipeline::load(input_ptr + offset);

    // For BF16, need to transform to compute domain
    typename ScalarPipeline::compute_register_type input_compute;
    if constexpr (std::is_same_v<value_type, bf16>) {
      input_compute = ScalarPipeline::transform_to(input_storage);
    } else {
      input_compute = input_storage;
    }

    // Perform operation
    auto result_compute = ScalarPipeline::op(input_compute, scalar_reg);

    // For BF16, transform back to storage domain
    typename ScalarPipeline::storage_register_type result_storage;
    if constexpr (std::is_same_v<value_type, bf16>) {
      result_storage = ScalarPipeline::transform_from(result_compute);
    } else {
      result_storage = result_compute;
    }

    // Store result
    ScalarPipeline::store(output_ptr + offset, result_storage);

    offset += step;
  }

  // Copy bitmask ONCE after the loop (scalar is always "present")
  output.present_mask() = input.present_mask();
}

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

  // Compute bitmask intersection ONCE after the loop using AVX2-optimized
  // bitset AND
  out.present_mask() = fst.present_mask();
  bitset_and_avx2(out.present_mask(), snd.present_mask());

  // Note: No tail handling needed - buffers are cache-line aligned by design.
  // The present_mask_ handles validity of elements within the padded buffer.
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

  // Compute bitmask intersection ONCE after the loop using AVX2-optimized
  // bitset AND
  bitset_and_avx2(mut.present_mask(), snd.present_mask());

  // Note: No tail handling needed - buffers are cache-line aligned by design.
  // The present_mask_ handles validity of elements within the padded buffer.
}

// Element-wise addition
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator+(const column_vector& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, Int32Pipeline<OpType::Add>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, float>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, Float32Pipeline<OpType::Add>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, BF16Pipeline<OpType::Add>>(*this, other, output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator+(column_vector&& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    vectorize_destructive<Policy, Int32Pipeline<OpType::Add>>(other, *this);
    return other;
  } else if constexpr (std::is_same_v<value_type, float>) {
    vectorize_destructive<Policy, Float32Pipeline<OpType::Add>>(other, *this);
    return other;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    vectorize_destructive<Policy, BF16Pipeline<OpType::Add>>(other, *this);
    return other;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

// Element-wise subtraction
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator-(const column_vector& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, Int32Pipeline<OpType::Sub>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, float>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, Float32Pipeline<OpType::Sub>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, BF16Pipeline<OpType::Sub>>(*this, other, output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator-(column_vector&& other) const {
  // Subtraction is non-commutative, so we can't reuse the rvalue buffer
  // since vectorize_destructive(other, *this) would compute other - *this
  // but we need *this - other. Just allocate a new result.
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);
    vectorize<Policy, Int32Pipeline<OpType::Sub>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, float>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);
    vectorize<Policy, Float32Pipeline<OpType::Sub>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);
    vectorize<Policy, BF16Pipeline<OpType::Sub>>(*this, other, output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

// Element-wise multiplication
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator*(const column_vector& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, Int32Pipeline<OpType::Mul>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, float>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, Float32Pipeline<OpType::Mul>>(*this, other, output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    const auto effective_size =
        std::min<std::size_t>(data_.size(), other.data_.size());
    column_vector<Policy> output(effective_size, allocator_);

    vectorize<Policy, BF16Pipeline<OpType::Mul>>(*this, other, output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator*(column_vector&& other) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    vectorize_destructive<Policy, Int32Pipeline<OpType::Mul>>(other, *this);
    return other;
  } else if constexpr (std::is_same_v<value_type, float>) {
    vectorize_destructive<Policy, Float32Pipeline<OpType::Mul>>(other, *this);
    return other;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    vectorize_destructive<Policy, BF16Pipeline<OpType::Mul>>(other, *this);
    return other;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

// Scalar addition: column + scalar
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator+(value_type scalar) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, Int32ScalarPipeline<OpType::Add>>(*this, scalar,
                                                               output);
    return output;
  } else if constexpr (std::is_same_v<value_type, float>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, Float32ScalarPipeline<OpType::Add>>(*this, scalar,
                                                                 output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, BF16ScalarPipeline<OpType::Add>>(*this, scalar,
                                                              output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

// Scalar subtraction: column - scalar
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator-(value_type scalar) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, Int32ScalarPipeline<OpType::Sub>>(*this, scalar,
                                                               output);
    return output;
  } else if constexpr (std::is_same_v<value_type, float>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, Float32ScalarPipeline<OpType::Sub>>(*this, scalar,
                                                                 output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, BF16ScalarPipeline<OpType::Sub>>(*this, scalar,
                                                              output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

// Scalar multiplication: column * scalar
template <concepts::ColumnPolicy Policy>
column_vector<Policy>
column_vector<Policy>::operator*(value_type scalar) const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, Int32ScalarPipeline<OpType::Mul>>(*this, scalar,
                                                               output);
    return output;
  } else if constexpr (std::is_same_v<value_type, float>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, Float32ScalarPipeline<OpType::Mul>>(*this, scalar,
                                                                 output);
    return output;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    column_vector<Policy> output(data_.size(), allocator_);
    vectorize_scalar<Policy, BF16ScalarPipeline<OpType::Mul>>(*this, scalar,
                                                              output);
    return output;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

// Friend operator implementation: scalar - column (non-commutative)
template <concepts::ColumnPolicy Policy>
column_vector<Policy> operator-(typename Policy::value_type scalar,
                                const column_vector<Policy>& col) {
  using value_type = typename Policy::value_type;

  // scalar - column requires us to compute (scalar - col[i]) for each element
  // This is equivalent to (-col[i] + scalar) which we can express as:
  // 1. Negate the column (multiply by -1)
  // 2. Add the scalar

  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    // Create a column filled with the scalar value
    column_vector<Policy> result(col.data().size(), col.allocator_);

    // Manually compute scalar - column using SIMD
    const value_type* col_ptr = col.data().data();
    value_type* result_ptr = result.data().data();

    auto scalar_reg = _mm256_set1_epi32(scalar);
    std::size_t offset = 0;
    const std::size_t step = 8;
    const std::size_t num_elements = col.data().size();

    while (offset + step <= num_elements) {
      auto col_reg =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(col_ptr + offset));
      auto result_reg = _mm256_sub_epi32(scalar_reg, col_reg);
      _mm256_store_si256(reinterpret_cast<__m256i*>(result_ptr + offset),
                         result_reg);

      offset += step;
    }

    // Copy bitmask ONCE after the loop
    result.present_mask() = col.present_mask();
    return result;
  } else if constexpr (std::is_same_v<value_type, float>) {
    column_vector<Policy> result(col.data().size(), col.allocator_);

    const value_type* col_ptr = col.data().data();
    value_type* result_ptr = result.data().data();

    auto scalar_reg = _mm256_set1_ps(scalar);
    std::size_t offset = 0;
    const std::size_t step = 8;
    const std::size_t num_elements = col.data().size();

    while (offset + step <= num_elements) {
      auto col_reg = _mm256_load_ps(col_ptr + offset);
      auto result_reg = _mm256_sub_ps(scalar_reg, col_reg);
      _mm256_store_ps(result_ptr + offset, result_reg);

      offset += step;
    }

    // Copy bitmask ONCE after the loop
    result.present_mask() = col.present_mask();
    return result;
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    column_vector<Policy> result(col.data().size(), col.allocator_);

    const value_type* col_ptr = col.data().data();
    value_type* result_ptr = result.data().data();

    float fp32_scalar = scalar.to_float();
    auto scalar_reg = _mm256_set1_ps(fp32_scalar);
    std::size_t offset = 0;
    const std::size_t step = 8;
    const std::size_t num_elements = col.data().size();

    while (offset + step <= num_elements) {
      auto col_bf16 =
          _mm_load_si128(reinterpret_cast<const __m128i*>(col_ptr + offset));
      auto col_fp32 = _mm256_cvtpbh_ps(reinterpret_cast<__m128bh>(col_bf16));
      auto result_fp32 = _mm256_sub_ps(scalar_reg, col_fp32);
      __m128bh result_bf16 = _mm256_cvtneps_pbh(result_fp32);
      _mm_store_si128(reinterpret_cast<__m128i*>(result_ptr + offset),
                      reinterpret_cast<__m128i>(result_bf16));

      offset += step;
    }

    // Copy bitmask ONCE after the loop
    result.present_mask() = col.present_mask();
    return result;
  } else {
    static_assert(!std::is_same_v<int, int>);
  }
}

template <concepts::ColumnPolicy Policy>
bool column_vector<Policy>::present(std::size_t index) const noexcept {
  if (index >= present_mask_.size()) [[unlikely]] {
    return false;
  }
  return present_mask_[index];
}

template <concepts::ColumnPolicy Policy>
bool column_vector<Policy>::present_unchecked(
    std::size_t index) const noexcept {
  FRANKLIN_DEBUG_ASSERT(index < present_mask_.size() &&
                        "present() index out of bounds");
  return present_mask_[index];
}

} // namespace franklin

#endif // FRANKLIN_CONTAINER_COLUMN_HPP
