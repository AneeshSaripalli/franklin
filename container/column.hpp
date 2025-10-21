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

  // Reduction operations - return identity value if empty or all missing
  value_type sum() const;
  value_type product() const;
  value_type min() const;
  value_type max() const;

  // Present mask operations
  bool any() const noexcept { return present_mask_.any(); }
  bool all() const noexcept { return present_mask_.all(); }
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
  // Create present_mask with all bits false, then set only valid elements to
  // true This avoids issues with padding bits being set to true
  present_mask_ = dynamic_bitset<BitsetPolicy>(rounded_size, false);
  for (std::size_t i = 0; i < size; ++i) {
    present_mask_.set(i, true);
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
  // Create present_mask with all bits false, then set only valid elements to
  // true This avoids issues with padding bits being set to true
  present_mask_ = dynamic_bitset<BitsetPolicy>(rounded_size, false);
  for (std::size_t i = 0; i < size; ++i) {
    present_mask_.set(i, true);
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

// Reduction operation types
enum class ReductionOp { Sum, Product, Min, Max };

// Helper: Extract 8 bits from dynamic_bitset at given bit index
FRANKLIN_FORCE_INLINE std::uint8_t
extract_8bits_from_bitset(const dynamic_bitset<BitsetPolicy>& bitset,
                          std::size_t bit_index) noexcept {
  std::size_t const block_idx = bit_index >> 6;
  std::size_t const bit_offset = bit_index & 63;
  return (bitset.blocks()[block_idx] >> bit_offset);
}

FRANKLIN_FORCE_INLINE __m256i expand_8bits_to_8x32bit_mask(uint8_t bits) {
  return _mm256_movm_epi32(bits);
}

// Horizontal sum for Int32
FRANKLIN_FORCE_INLINE int32_t horizontal_sum_epi32(__m256i vec) {
  // Hadd twice to get 4 sums, then extract and sum
  __m256i sum1 =
      _mm256_hadd_epi32(vec, vec); // [a+b, c+d, a+b, c+d, e+f, g+h, e+f, g+h]
  __m256i sum2 = _mm256_hadd_epi32(sum1, sum1); // [a+b+c+d, ..., e+f+g+h, ...]

  // Extract low and high 128-bit lanes
  __m128i low = _mm256_castsi256_si128(sum2);
  __m128i high = _mm256_extracti128_si256(sum2, 1);

  // Add low and high lanes
  __m128i sum = _mm_add_epi32(low, high);

  // Extract final sum
  return _mm_cvtsi128_si32(sum);
}

// Horizontal sum for Float32
FRANKLIN_FORCE_INLINE float horizontal_sum_ps(__m256 vec) {
  // Hadd twice
  __m256 sum1 = _mm256_hadd_ps(vec, vec);
  __m256 sum2 = _mm256_hadd_ps(sum1, sum1);

  // Extract low and high lanes
  __m128 low = _mm256_castps256_ps128(sum2);
  __m128 high = _mm256_extractf128_ps(sum2, 1);

  // Add and extract
  __m128 sum = _mm_add_ps(low, high);
  return _mm_cvtss_f32(sum);
}

// Horizontal min for Int32
FRANKLIN_FORCE_INLINE int32_t horizontal_min_epi32(__m256i vec) {
  // Use pairwise min operations
  __m128i low = _mm256_castsi256_si128(vec);
  __m128i high = _mm256_extracti128_si256(vec, 1);
  __m128i min128 = _mm_min_epi32(low, high);

  // Reduce 128-bit to scalar
  __m128i min64 =
      _mm_min_epi32(min128, _mm_shuffle_epi32(min128, _MM_SHUFFLE(1, 0, 3, 2)));
  __m128i min32 =
      _mm_min_epi32(min64, _mm_shuffle_epi32(min64, _MM_SHUFFLE(2, 3, 0, 1)));

  return _mm_cvtsi128_si32(min32);
}

// Horizontal max for Int32
FRANKLIN_FORCE_INLINE int32_t horizontal_max_epi32(__m256i vec) {
  __m128i low = _mm256_castsi256_si128(vec);
  __m128i high = _mm256_extracti128_si256(vec, 1);
  __m128i max128 = _mm_max_epi32(low, high);

  __m128i max64 =
      _mm_max_epi32(max128, _mm_shuffle_epi32(max128, _MM_SHUFFLE(1, 0, 3, 2)));
  __m128i max32 =
      _mm_max_epi32(max64, _mm_shuffle_epi32(max64, _MM_SHUFFLE(2, 3, 0, 1)));

  return _mm_cvtsi128_si32(max32);
}

// Horizontal min for Float32
FRANKLIN_FORCE_INLINE float horizontal_min_ps(__m256 vec) {
  __m128 low = _mm256_castps256_ps128(vec);
  __m128 high = _mm256_extractf128_ps(vec, 1);
  __m128 min128 = _mm_min_ps(low, high);

  __m128 min64 = _mm_min_ps(
      min128, _mm_shuffle_ps(min128, min128, _MM_SHUFFLE(1, 0, 3, 2)));
  __m128 min32 =
      _mm_min_ps(min64, _mm_shuffle_ps(min64, min64, _MM_SHUFFLE(2, 3, 0, 1)));

  return _mm_cvtss_f32(min32);
}

// Horizontal max for Float32
FRANKLIN_FORCE_INLINE float horizontal_max_ps(__m256 vec) {
  __m128 low = _mm256_castps256_ps128(vec);
  __m128 high = _mm256_extractf128_ps(vec, 1);
  __m128 max128 = _mm_max_ps(low, high);

  __m128 max64 = _mm_max_ps(
      max128, _mm_shuffle_ps(max128, max128, _MM_SHUFFLE(1, 0, 3, 2)));
  __m128 max32 =
      _mm_max_ps(max64, _mm_shuffle_ps(max64, max64, _MM_SHUFFLE(2, 3, 0, 1)));

  return _mm_cvtss_f32(max32);
}

// Int32 SIMD reduction
template <ReductionOp Op>
FRANKLIN_FORCE_INLINE int32_t
reduce_int32(const int32_t* data, const dynamic_bitset<BitsetPolicy>& mask,
             std::size_t num_elements) {
  // Initialize accumulator with identity
  int32_t identity;
  if constexpr (Op == ReductionOp::Sum) {
    identity = 0;
  } else if constexpr (Op == ReductionOp::Product) {
    identity = 1;
  } else if constexpr (Op == ReductionOp::Min) {
    identity = std::numeric_limits<int32_t>::max();
  } else if constexpr (Op == ReductionOp::Max) {
    identity = std::numeric_limits<int32_t>::lowest();
  }

  __m256i identity_vec = _mm256_set1_epi32(identity);
  __m256i accumulator = identity_vec;

  std::size_t i = 0;
  const std::size_t num_full_vecs = num_elements / 8;

  // Main loop: process 8 elements at a time with bitmask blending
  for (std::size_t vec_idx = 0; vec_idx < num_full_vecs; ++vec_idx, i += 8) {
    // Load 8 elements
    __m256i data_vec =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i));

    std::uint8_t const mask_bits = extract_8bits_from_bitset(mask, i);

    // Convert to SIMD mask
    auto const simd_mask = expand_8bits_to_8x32bit_mask(mask_bits);

    // Blend: where mask==0, use identity; where mask==0xFFFFFFFF, use data
    __m256i blended = _mm256_blendv_epi8(identity_vec, data_vec, simd_mask);

    // Accumulate
    if constexpr (Op == ReductionOp::Sum) {
      accumulator = _mm256_add_epi32(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Product) {
      accumulator = _mm256_mullo_epi32(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Min) {
      accumulator = _mm256_min_epi32(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Max) {
      accumulator = _mm256_max_epi32(accumulator, blended);
    }
  }

  // Tail: handle remaining < 8 elements
  if (i < num_elements) {
    std::size_t tail_count = num_elements - i;

    // Create mask for tail elements
    alignas(32) int32_t tail_mask[8] = {0};
    for (std::size_t j = 0; j < tail_count; ++j) {
      if (mask[i + j]) {
        tail_mask[j] = -1; // 0xFFFFFFFF
      }
    }
    __m256i simd_tail_mask =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(tail_mask));

    // Load tail data (may read past end, but we'll mask it out)
    __m256i tail_data =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i));
    __m256i blended_tail =
        _mm256_blendv_epi8(identity_vec, tail_data, simd_tail_mask);

    // Accumulate tail
    if constexpr (Op == ReductionOp::Sum) {
      accumulator = _mm256_add_epi32(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Product) {
      accumulator = _mm256_mullo_epi32(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Min) {
      accumulator = _mm256_min_epi32(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Max) {
      accumulator = _mm256_max_epi32(accumulator, blended_tail);
    }
  }

  // Horizontal reduction
  if constexpr (Op == ReductionOp::Sum) {
    return horizontal_sum_epi32(accumulator);
  } else if constexpr (Op == ReductionOp::Product) {
    // For product, need to multiply all lanes
    alignas(32) int32_t lanes[8];
    _mm256_store_si256(reinterpret_cast<__m256i*>(lanes), accumulator);
    int32_t result = 1;
    for (int j = 0; j < 8; ++j) {
      result *= lanes[j];
    }
    return result;
  } else if constexpr (Op == ReductionOp::Min) {
    return horizontal_min_epi32(accumulator);
  } else if constexpr (Op == ReductionOp::Max) {
    return horizontal_max_epi32(accumulator);
  }
}

// Float32 SIMD reduction
template <ReductionOp Op>
FRANKLIN_FORCE_INLINE float
reduce_float32(const float* data, const dynamic_bitset<BitsetPolicy>& mask,
               std::size_t num_elements) {
  // Initialize with identity
  float identity;
  if constexpr (Op == ReductionOp::Sum) {
    identity = 0.0f;
  } else if constexpr (Op == ReductionOp::Product) {
    identity = 1.0f;
  } else if constexpr (Op == ReductionOp::Min) {
    identity = std::numeric_limits<float>::max();
  } else if constexpr (Op == ReductionOp::Max) {
    identity = std::numeric_limits<float>::lowest();
  }

  __m256 identity_vec = _mm256_set1_ps(identity);
  __m256 accumulator = identity_vec;

  std::size_t i = 0;
  const std::size_t num_full_vecs = num_elements / 8;

  // Main loop
  for (std::size_t vec_idx = 0; vec_idx < num_full_vecs; ++vec_idx, i += 8) {
    __m256 data_vec = _mm256_load_ps(data + i);

    std::uint8_t const mask_bits = extract_8bits_from_bitset(mask, i);
    auto const simd_mask =
        _mm256_castsi256_ps(expand_8bits_to_8x32bit_mask(mask_bits));

    __m256 blended = _mm256_blendv_ps(identity_vec, data_vec, simd_mask);

    if constexpr (Op == ReductionOp::Sum) {
      accumulator = _mm256_add_ps(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Product) {
      accumulator = _mm256_mul_ps(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Min) {
      accumulator = _mm256_min_ps(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Max) {
      accumulator = _mm256_max_ps(accumulator, blended);
    }
  }

  // Tail
  if (i < num_elements) {
    std::size_t tail_count = num_elements - i;
    alignas(32) int32_t tail_mask[8] = {0};
    for (std::size_t j = 0; j < tail_count; ++j) {
      if (mask[i + j]) {
        tail_mask[j] = -1;
      }
    }
    __m256 simd_tail_mask = _mm256_castsi256_ps(
        _mm256_load_si256(reinterpret_cast<const __m256i*>(tail_mask)));

    __m256 tail_data = _mm256_load_ps(data + i);
    __m256 blended_tail =
        _mm256_blendv_ps(identity_vec, tail_data, simd_tail_mask);

    if constexpr (Op == ReductionOp::Sum) {
      accumulator = _mm256_add_ps(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Product) {
      accumulator = _mm256_mul_ps(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Min) {
      accumulator = _mm256_min_ps(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Max) {
      accumulator = _mm256_max_ps(accumulator, blended_tail);
    }
  }

  // Horizontal reduction
  if constexpr (Op == ReductionOp::Sum) {
    return horizontal_sum_ps(accumulator);
  } else if constexpr (Op == ReductionOp::Product) {
    alignas(32) float lanes[8];
    _mm256_store_ps(lanes, accumulator);
    float result = 1.0f;
    for (int j = 0; j < 8; ++j) {
      result *= lanes[j];
    }
    return result;
  } else if constexpr (Op == ReductionOp::Min) {
    return horizontal_min_ps(accumulator);
  } else if constexpr (Op == ReductionOp::Max) {
    return horizontal_max_ps(accumulator);
  }
}

// BF16 SIMD reduction (converts to FP32 for computation)
template <ReductionOp Op>
FRANKLIN_FORCE_INLINE bf16 reduce_bf16(const bf16* data,
                                       const dynamic_bitset<BitsetPolicy>& mask,
                                       std::size_t num_elements) {
  // Work in FP32 domain
  float identity;
  if constexpr (Op == ReductionOp::Sum) {
    identity = 0.0f;
  } else if constexpr (Op == ReductionOp::Product) {
    identity = 1.0f;
  } else if constexpr (Op == ReductionOp::Min) {
    identity = std::numeric_limits<float>::max();
  } else if constexpr (Op == ReductionOp::Max) {
    identity = std::numeric_limits<float>::lowest();
  }

  __m256 identity_vec = _mm256_set1_ps(identity);
  __m256 accumulator = identity_vec;

  std::size_t i = 0;
  const std::size_t num_full_vecs = num_elements / 8;

  // Main loop
  for (std::size_t vec_idx = 0; vec_idx < num_full_vecs; ++vec_idx, i += 8) {
    // Load 8 bf16 values (16 bytes)
    __m128i bf16_data =
        _mm_load_si128(reinterpret_cast<const __m128i*>(data + i));

    // Convert to FP32
    __m256 fp32_data = _mm256_cvtpbh_ps(reinterpret_cast<__m128bh>(bf16_data));

    // Get mask and blend
    std::uint8_t const mask_bits = extract_8bits_from_bitset(mask, i);
    auto const simd_mask =
        _mm256_castsi256_ps(expand_8bits_to_8x32bit_mask(mask_bits));

    __m256 blended = _mm256_blendv_ps(identity_vec, fp32_data, simd_mask);

    if constexpr (Op == ReductionOp::Sum) {
      accumulator = _mm256_add_ps(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Product) {
      accumulator = _mm256_mul_ps(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Min) {
      accumulator = _mm256_min_ps(accumulator, blended);
    } else if constexpr (Op == ReductionOp::Max) {
      accumulator = _mm256_max_ps(accumulator, blended);
    }
  }

  // Tail
  if (i < num_elements) {
    std::size_t tail_count = num_elements - i;
    alignas(32) int32_t tail_mask[8] = {0};
    for (std::size_t j = 0; j < tail_count; ++j) {
      if (mask[i + j]) {
        tail_mask[j] = -1;
      }
    }
    __m256 simd_tail_mask = _mm256_castsi256_ps(
        _mm256_load_si256(reinterpret_cast<const __m256i*>(tail_mask)));

    __m128i bf16_tail =
        _mm_load_si128(reinterpret_cast<const __m128i*>(data + i));
    __m256 fp32_tail = _mm256_cvtpbh_ps(reinterpret_cast<__m128bh>(bf16_tail));
    __m256 blended_tail =
        _mm256_blendv_ps(identity_vec, fp32_tail, simd_tail_mask);

    if constexpr (Op == ReductionOp::Sum) {
      accumulator = _mm256_add_ps(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Product) {
      accumulator = _mm256_mul_ps(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Min) {
      accumulator = _mm256_min_ps(accumulator, blended_tail);
    } else if constexpr (Op == ReductionOp::Max) {
      accumulator = _mm256_max_ps(accumulator, blended_tail);
    }
  }

  // Horizontal reduction to scalar FP32
  float fp32_result;
  if constexpr (Op == ReductionOp::Sum) {
    fp32_result = horizontal_sum_ps(accumulator);
  } else if constexpr (Op == ReductionOp::Product) {
    alignas(32) float lanes[8];
    _mm256_store_ps(lanes, accumulator);
    fp32_result = 1.0f;
    for (int j = 0; j < 8; ++j) {
      fp32_result *= lanes[j];
    }
  } else if constexpr (Op == ReductionOp::Min) {
    fp32_result = horizontal_min_ps(accumulator);
  } else if constexpr (Op == ReductionOp::Max) {
    fp32_result = horizontal_max_ps(accumulator);
  }

  // Convert back to BF16
  return bf16::from_float(fp32_result);
}

// Vectorized scalar operation: column op scalar
template <concepts::ColumnPolicy ColPolicy, typename ScalarPipeline>
static void vectorize_scalar(column_vector<ColPolicy> const& input,
                             typename ColPolicy::value_type scalar,
                             column_vector<ColPolicy>& output) {
  using value_type = typename ColPolicy::value_type;
  // OPTIMIZATION: Use __restrict__ to tell compiler pointers don't alias
  const value_type* __restrict input_ptr = input.data().data();
  value_type* __restrict output_ptr = output.data().data();

  std::size_t offset = 0;
  const std::size_t step = ScalarPipeline::elements_per_iteration;
  const std::size_t num_elements = output.data().size();

  // Broadcast scalar to all lanes ONCE outside the loop
  auto scalar_reg = ScalarPipeline::broadcast(scalar);

  // OPTIMIZATION: Manual loop unrolling (4x) for instruction-level parallelism
  const std::size_t unroll_step = step * 4;

  while (offset + unroll_step <= num_elements) {
    // Unroll 1
    auto in1_storage = ScalarPipeline::load(input_ptr + offset);
    typename ScalarPipeline::compute_register_type in1_compute;
    if constexpr (std::is_same_v<value_type, bf16>) {
      in1_compute = ScalarPipeline::transform_to(in1_storage);
    } else {
      in1_compute = in1_storage;
    }
    auto r1_compute = ScalarPipeline::op(in1_compute, scalar_reg);

    // Unroll 2
    auto in2_storage = ScalarPipeline::load(input_ptr + offset + step);
    typename ScalarPipeline::compute_register_type in2_compute;
    if constexpr (std::is_same_v<value_type, bf16>) {
      in2_compute = ScalarPipeline::transform_to(in2_storage);
    } else {
      in2_compute = in2_storage;
    }
    auto r2_compute = ScalarPipeline::op(in2_compute, scalar_reg);

    // Unroll 3
    auto in3_storage = ScalarPipeline::load(input_ptr + offset + 2 * step);
    typename ScalarPipeline::compute_register_type in3_compute;
    if constexpr (std::is_same_v<value_type, bf16>) {
      in3_compute = ScalarPipeline::transform_to(in3_storage);
    } else {
      in3_compute = in3_storage;
    }
    auto r3_compute = ScalarPipeline::op(in3_compute, scalar_reg);

    // Unroll 4
    auto in4_storage = ScalarPipeline::load(input_ptr + offset + 3 * step);
    typename ScalarPipeline::compute_register_type in4_compute;
    if constexpr (std::is_same_v<value_type, bf16>) {
      in4_compute = ScalarPipeline::transform_to(in4_storage);
    } else {
      in4_compute = in4_storage;
    }
    auto r4_compute = ScalarPipeline::op(in4_compute, scalar_reg);

    // Transform back and store (BF16 only)
    typename ScalarPipeline::storage_register_type r1_storage, r2_storage,
        r3_storage, r4_storage;
    if constexpr (std::is_same_v<value_type, bf16>) {
      r1_storage = ScalarPipeline::transform_from(r1_compute);
      r2_storage = ScalarPipeline::transform_from(r2_compute);
      r3_storage = ScalarPipeline::transform_from(r3_compute);
      r4_storage = ScalarPipeline::transform_from(r4_compute);
    } else {
      r1_storage = r1_compute;
      r2_storage = r2_compute;
      r3_storage = r3_compute;
      r4_storage = r4_compute;
    }

    ScalarPipeline::store(output_ptr + offset, r1_storage);
    ScalarPipeline::store(output_ptr + offset + step, r2_storage);
    ScalarPipeline::store(output_ptr + offset + 2 * step, r3_storage);
    ScalarPipeline::store(output_ptr + offset + 3 * step, r4_storage);

    offset += unroll_step;
  }

  // Handle remaining iterations
  while (offset + step <= num_elements) {
    auto input_storage = ScalarPipeline::load(input_ptr + offset);

    typename ScalarPipeline::compute_register_type input_compute;
    if constexpr (std::is_same_v<value_type, bf16>) {
      input_compute = ScalarPipeline::transform_to(input_storage);
    } else {
      input_compute = input_storage;
    }

    auto result_compute = ScalarPipeline::op(input_compute, scalar_reg);

    typename ScalarPipeline::storage_register_type result_storage;
    if constexpr (std::is_same_v<value_type, bf16>) {
      result_storage = ScalarPipeline::transform_from(result_compute);
    } else {
      result_storage = result_compute;
    }

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
  // OPTIMIZATION: Use __restrict__ to tell compiler pointers don't alias
  // This enables aggressive load/store reordering and better pipelining
  const value_type* __restrict fst_ptr = fst.data().data();
  const value_type* __restrict snd_ptr = snd.data().data();
  value_type* __restrict out_ptr = out.data().data();

  std::size_t offset = 0;
  const std::size_t step = Pipeline::elements_per_iteration;
  const std::size_t num_elements = out.data().size();

  // OPTIMIZATION: Manual loop unrolling (4x) for instruction-level parallelism
  // This creates 4 independent load->compute->store chains that can execute
  // in parallel on modern out-of-order CPUs, reducing critical path latency
  const std::size_t unroll_step = step * 4;

  while (offset + unroll_step <= num_elements) {
    // Unroll 1: Process elements [offset, offset+step)
    auto a1_storage = Pipeline::load(fst_ptr + offset);
    auto b1_storage = Pipeline::load(snd_ptr + offset);
    auto a1_compute = Pipeline::transform_to(a1_storage);
    auto b1_compute = Pipeline::transform_to(b1_storage);
    auto r1_compute = Pipeline::op(a1_compute, b1_compute);
    auto r1_storage = Pipeline::transform_from(r1_compute);

    // Unroll 2: Process elements [offset+step, offset+2*step)
    auto a2_storage = Pipeline::load(fst_ptr + offset + step);
    auto b2_storage = Pipeline::load(snd_ptr + offset + step);
    auto a2_compute = Pipeline::transform_to(a2_storage);
    auto b2_compute = Pipeline::transform_to(b2_storage);
    auto r2_compute = Pipeline::op(a2_compute, b2_compute);
    auto r2_storage = Pipeline::transform_from(r2_compute);

    // Unroll 3: Process elements [offset+2*step, offset+3*step)
    auto a3_storage = Pipeline::load(fst_ptr + offset + 2 * step);
    auto b3_storage = Pipeline::load(snd_ptr + offset + 2 * step);
    auto a3_compute = Pipeline::transform_to(a3_storage);
    auto b3_compute = Pipeline::transform_to(b3_storage);
    auto r3_compute = Pipeline::op(a3_compute, b3_compute);
    auto r3_storage = Pipeline::transform_from(r3_compute);

    // Unroll 4: Process elements [offset+3*step, offset+4*step)
    auto a4_storage = Pipeline::load(fst_ptr + offset + 3 * step);
    auto b4_storage = Pipeline::load(snd_ptr + offset + 3 * step);
    auto a4_compute = Pipeline::transform_to(a4_storage);
    auto b4_compute = Pipeline::transform_to(b4_storage);
    auto r4_compute = Pipeline::op(a4_compute, b4_compute);
    auto r4_storage = Pipeline::transform_from(r4_compute);

    // Store all results (can pipeline with loads from next iteration)
    Pipeline::store(out_ptr + offset, r1_storage);
    Pipeline::store(out_ptr + offset + step, r2_storage);
    Pipeline::store(out_ptr + offset + 2 * step, r3_storage);
    Pipeline::store(out_ptr + offset + 3 * step, r4_storage);

    offset += unroll_step;
  }

  // Handle remaining iterations with single-iteration loop
  while (offset + step <= num_elements) {
    auto a_storage = Pipeline::load(fst_ptr + offset);
    auto b_storage = Pipeline::load(snd_ptr + offset);
    auto a_compute = Pipeline::transform_to(a_storage);
    auto b_compute = Pipeline::transform_to(b_storage);
    auto result_compute = Pipeline::op(a_compute, b_compute);
    auto result_storage = Pipeline::transform_from(result_compute);
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
  // OPTIMIZATION: Use __restrict__ for non-aliasing pointers
  value_type* __restrict mut_ptr = mut.data().data();
  const value_type* __restrict snd_ptr = snd.data().data();

  std::size_t offset = 0;
  const std::size_t step = Pipeline::elements_per_iteration;
  const std::size_t num_elements =
      std::min(mut.data().size(), snd.data().size());

  // OPTIMIZATION: Manual loop unrolling (4x)
  const std::size_t unroll_step = step * 4;

  while (offset + unroll_step <= num_elements) {
    // Unroll 1
    auto a1_storage = Pipeline::load(mut_ptr + offset);
    auto b1_storage = Pipeline::load(snd_ptr + offset);
    auto a1_compute = Pipeline::transform_to(a1_storage);
    auto b1_compute = Pipeline::transform_to(b1_storage);
    auto r1_compute = Pipeline::op(a1_compute, b1_compute);
    auto r1_storage = Pipeline::transform_from(r1_compute);

    // Unroll 2
    auto a2_storage = Pipeline::load(mut_ptr + offset + step);
    auto b2_storage = Pipeline::load(snd_ptr + offset + step);
    auto a2_compute = Pipeline::transform_to(a2_storage);
    auto b2_compute = Pipeline::transform_to(b2_storage);
    auto r2_compute = Pipeline::op(a2_compute, b2_compute);
    auto r2_storage = Pipeline::transform_from(r2_compute);

    // Unroll 3
    auto a3_storage = Pipeline::load(mut_ptr + offset + 2 * step);
    auto b3_storage = Pipeline::load(snd_ptr + offset + 2 * step);
    auto a3_compute = Pipeline::transform_to(a3_storage);
    auto b3_compute = Pipeline::transform_to(b3_storage);
    auto r3_compute = Pipeline::op(a3_compute, b3_compute);
    auto r3_storage = Pipeline::transform_from(r3_compute);

    // Unroll 4
    auto a4_storage = Pipeline::load(mut_ptr + offset + 3 * step);
    auto b4_storage = Pipeline::load(snd_ptr + offset + 3 * step);
    auto a4_compute = Pipeline::transform_to(a4_storage);
    auto b4_compute = Pipeline::transform_to(b4_storage);
    auto r4_compute = Pipeline::op(a4_compute, b4_compute);
    auto r4_storage = Pipeline::transform_from(r4_compute);

    // Store all results
    Pipeline::store(mut_ptr + offset, r1_storage);
    Pipeline::store(mut_ptr + offset + step, r2_storage);
    Pipeline::store(mut_ptr + offset + 2 * step, r3_storage);
    Pipeline::store(mut_ptr + offset + 3 * step, r4_storage);

    offset += unroll_step;
  }

  // Handle remaining iterations
  while (offset + step <= num_elements) {
    auto a_storage = Pipeline::load(mut_ptr + offset);
    auto b_storage = Pipeline::load(snd_ptr + offset);
    auto a_compute = Pipeline::transform_to(a_storage);
    auto b_compute = Pipeline::transform_to(b_storage);
    auto result_compute = Pipeline::op(a_compute, b_compute);
    auto result_storage = Pipeline::transform_from(result_compute);
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

// Reduction operation implementations
template <concepts::ColumnPolicy Policy>
typename Policy::value_type column_vector<Policy>::sum() const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    return reduce_int32<ReductionOp::Sum>(data_.data(), present_mask_,
                                          data_.size());
  } else if constexpr (std::is_same_v<value_type, float>) {
    return reduce_float32<ReductionOp::Sum>(data_.data(), present_mask_,
                                            data_.size());
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    return reduce_bf16<ReductionOp::Sum>(data_.data(), present_mask_,
                                         data_.size());
  } else {
    static_assert(!std::is_same_v<int, int>, "Unsupported type for sum()");
  }
}

template <concepts::ColumnPolicy Policy>
typename Policy::value_type column_vector<Policy>::product() const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    return reduce_int32<ReductionOp::Product>(data_.data(), present_mask_,
                                              data_.size());
  } else if constexpr (std::is_same_v<value_type, float>) {
    return reduce_float32<ReductionOp::Product>(data_.data(), present_mask_,
                                                data_.size());
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    return reduce_bf16<ReductionOp::Product>(data_.data(), present_mask_,
                                             data_.size());
  } else {
    static_assert(!std::is_same_v<int, int>, "Unsupported type for product()");
  }
}

template <concepts::ColumnPolicy Policy>
typename Policy::value_type column_vector<Policy>::min() const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    return reduce_int32<ReductionOp::Min>(data_.data(), present_mask_,
                                          data_.size());
  } else if constexpr (std::is_same_v<value_type, float>) {
    return reduce_float32<ReductionOp::Min>(data_.data(), present_mask_,
                                            data_.size());
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    return reduce_bf16<ReductionOp::Min>(data_.data(), present_mask_,
                                         data_.size());
  } else {
    static_assert(!std::is_same_v<int, int>, "Unsupported type for min()");
  }
}

template <concepts::ColumnPolicy Policy>
typename Policy::value_type column_vector<Policy>::max() const {
  if constexpr (std::is_same_v<value_type, std::int32_t>) {
    return reduce_int32<ReductionOp::Max>(data_.data(), present_mask_,
                                          data_.size());
  } else if constexpr (std::is_same_v<value_type, float>) {
    return reduce_float32<ReductionOp::Max>(data_.data(), present_mask_,
                                            data_.size());
  } else if constexpr (std::is_same_v<value_type, bf16>) {
    return reduce_bf16<ReductionOp::Max>(data_.data(), present_mask_,
                                         data_.size());
  } else {
    static_assert(!std::is_same_v<int, int>, "Unsupported type for max()");
  }
}

} // namespace franklin

#endif // FRANKLIN_CONTAINER_COLUMN_HPP
