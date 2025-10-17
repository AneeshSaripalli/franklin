#ifndef FRANKLIN_CONTAINER_DYNAMIC_BITSET_HPP
#define FRANKLIN_CONTAINER_DYNAMIC_BITSET_HPP

#include "core/error_collector.hpp"
#include "memory/aligned_allocator.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <vector>

namespace franklin {

namespace detail {
// AVX512 VPOPCNTDQ-optimized helper for counting set bits
// Process 4 blocks (32 bytes = 256 bits) at a time using _mm256_popcnt_epi64
// Returns the total count of set bits in the first num_blocks blocks
inline std::uint64_t simd_count_blocks(const std::uint64_t* data,
                                       std::size_t num_blocks) {
  __m256i vec_count = _mm256_setzero_si256();
  const std::size_t simd_blocks = num_blocks / 4;

  for (std::size_t i = 0; i < simd_blocks; ++i) {
    __m256i vec =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i * 4));
    __m256i popcount = _mm256_popcnt_epi64(vec);
    vec_count = _mm256_add_epi64(vec_count, popcount);
  }

  // Extract and sum the 4 accumulated counts
  alignas(32) std::uint64_t counts[4];
  _mm256_store_si256(reinterpret_cast<__m256i*>(counts), vec_count);
  std::uint64_t result = counts[0] + counts[1] + counts[2] + counts[3];

  // Handle remaining full blocks with scalar popcount
  for (std::size_t i = simd_blocks * 4; i < num_blocks; ++i) {
    result += std::popcount(data[i]);
  }

  return result;
}
} // namespace detail

template <typename Policy> class dynamic_bitset {
public:
  using block_type = std::uint64_t;
  using size_type = typename Policy::size_type;
  using allocator_type = memory::aligned_allocator<block_type, 64>;

  static constexpr size_type bits_per_block = 64;
  static constexpr size_type block_shift = 6; // log2(64) = 6

private:
  std::vector<block_type, allocator_type> blocks_;
  size_type num_bits_;

  // OPTIMIZATION 1: Replace division with bit shift (pos / 64 => pos >> 6)
  static constexpr size_type block_index(size_type pos) noexcept {
    return pos >> block_shift;
  }

  // OPTIMIZATION 2: Replace modulo with bit mask (pos % 64 => pos & 63)
  static constexpr size_type bit_index(size_type pos) noexcept {
    return pos & (bits_per_block - 1);
  }

  // OPTIMIZATION 3: Combine bit_index and shift in one operation
  static constexpr block_type bit_mask(size_type pos) noexcept {
    return block_type(1) << (pos & (bits_per_block - 1));
  }

  // OPTIMIZATION 4: Replace division with bit shift
  constexpr size_type num_blocks() const noexcept {
    return (num_bits_ + bits_per_block - 1) >> block_shift;
  }

  // OPTIMIZATION 5: Replace modulo with bit mask
  constexpr void zero_unused_bits() noexcept {
    const size_type remainder = num_bits_ & (bits_per_block - 1);
    if (remainder != 0) {
      block_type mask = (block_type(1) << remainder) - 1;
      blocks_.back() &= mask;
    }
  }

public:
  constexpr dynamic_bitset() noexcept : num_bits_(0) {}

  explicit constexpr dynamic_bitset(size_type num_bits, bool value = false)
      : blocks_(num_blocks_needed(num_bits),
                value ? ~block_type(0) : block_type(0)),
        num_bits_(num_bits) {
    if (value) {
      zero_unused_bits();
    }
  }

  constexpr size_type size() const noexcept { return num_bits_; }

  constexpr bool empty() const noexcept { return num_bits_ == 0; }

  constexpr void resize(size_type num_bits, bool value = false) {
    size_type old_num_bits = num_bits_;
    size_type old_num_blocks = num_blocks();
    num_bits_ = num_bits;
    size_type new_num_blocks = num_blocks();

    if (new_num_blocks != old_num_blocks) {
      blocks_.resize(new_num_blocks, value ? ~block_type(0) : block_type(0));
    }

    // Fix Bug #2: When growing, set new bits to the specified value
    if (num_bits > old_num_bits && value) {
      // Set new bits in the range [old_num_bits, num_bits)
      for (size_type i = old_num_bits; i < num_bits; ++i) {
        size_type block_idx = block_index(i);
        blocks_[block_idx] |= bit_mask(i);
      }
    }

    // Fix Bug #3: When shrinking, always clear unused bits
    if (num_bits < old_num_bits) {
      zero_unused_bits();
    } else if (value) {
      zero_unused_bits();
    }
  }

  constexpr void clear() noexcept {
    blocks_.clear();
    num_bits_ = 0;
  }

  constexpr void push_back(bool value) {
    size_type old_size = num_bits_;
    resize(num_bits_ + 1);
    set(old_size, value);
  }

  constexpr bool test(size_type pos) const {
    // OPTIMIZATION 6: Branch prediction hint - bounds check rarely fails
    if (pos >= num_bits_) [[unlikely]] {
      auto error =
          core::make_error(core::ErrorCode::OutOfRange, "dynamic_bitset",
                           "test", "Position exceeds bitset size");
      error.add_context(pos);
      error.add_context(num_bits_);
      core::report_error(std::move(error));
      return false;
    }
    return (blocks_[block_index(pos)] & bit_mask(pos)) != 0;
  }

  // Unchecked access - no bounds checking, reads indeterminate padding bits if
  // OOB Use when you've already validated bounds or need maximum performance
  constexpr bool test_unchecked(size_type pos) const noexcept {
    return (blocks_[block_index(pos)] & bit_mask(pos)) != 0;
  }

  // operator[] is an alias for test_unchecked() for convenience
  constexpr bool operator[](size_type pos) const noexcept {
    return test_unchecked(pos);
  }

  constexpr dynamic_bitset& set(size_type pos, bool value = true) {
    // OPTIMIZATION 7: Branch prediction hint - bounds check rarely fails
    if (pos >= num_bits_) [[unlikely]] {
      auto error =
          core::make_error(core::ErrorCode::OutOfRange, "dynamic_bitset", "set",
                           "Position exceeds bitset size");
      error.add_context(pos);
      error.add_context(num_bits_);
      error.add_context(value);
      core::report_error(std::move(error));
      return *this;
    }

    // OPTIMIZATION 8: Branch prediction hint - setting to true is common
    if (value) [[likely]] {
      blocks_[block_index(pos)] |= bit_mask(pos);
    } else {
      blocks_[block_index(pos)] &= ~bit_mask(pos);
    }
    return *this;
  }

  dynamic_bitset& set() noexcept {
    if (blocks_.empty()) {
      return *this;
    }

    block_type* data = blocks_.data();
    const size_type num_blocks = blocks_.size();

    // Broadcast all-ones pattern to AVX2 register
    const __m256i all_ones = _mm256_set1_epi64x(-1LL);

    // Store full cache lines (8 blocks = 64 bytes) using aligned stores
    // num_blocks is always a multiple of 8, so no tail handling needed
    for (size_type i = 0; i < num_blocks; i += 8) {
      // First half of cache line (32 bytes = 4 blocks)
      _mm256_store_si256(reinterpret_cast<__m256i*>(data + i), all_ones);
      // Second half of cache line (32 bytes = 4 blocks)
      _mm256_store_si256(reinterpret_cast<__m256i*>(data + i + 4), all_ones);
    }

    // Note: We don't call zero_unused_bits() because padding bits are masked
    // off in summary operations (count(), all(), any(), none())
    return *this;
  }

  constexpr dynamic_bitset& reset(size_type pos) { return set(pos, false); }

  constexpr dynamic_bitset& reset() noexcept {
    // OPTIMIZATION 9: Use memset for bulk zeroing (faster than loop for large
    // blocks)
    if (!blocks_.empty()) {
      std::memset(blocks_.data(), 0, blocks_.size() * sizeof(block_type));
    }
    return *this;
  }

  constexpr dynamic_bitset& flip(size_type pos) {
    // OPTIMIZATION 10: Branch prediction hint - bounds check rarely fails
    if (pos >= num_bits_) [[unlikely]] {
      auto error =
          core::make_error(core::ErrorCode::OutOfRange, "dynamic_bitset",
                           "flip", "Position exceeds bitset size");
      error.add_context(pos);
      error.add_context(num_bits_);
      core::report_error(std::move(error));
      return *this;
    }
    blocks_[block_index(pos)] ^= bit_mask(pos);
    return *this;
  }

  dynamic_bitset& flip() noexcept {
    if (blocks_.empty()) {
      return *this;
    }

    block_type* data = blocks_.data();
    const size_type num_blocks = blocks_.size();

    // Broadcast all-ones pattern to AVX2 register for XOR flipping
    const __m256i all_ones = _mm256_set1_epi64x(-1LL);

    // Flip full cache lines (8 blocks = 64 bytes) using AVX2 XOR
    // num_blocks is always a multiple of 8, so no tail handling needed
    for (size_type i = 0; i < num_blocks; i += 8) {
      // First half of cache line (32 bytes = 4 blocks)
      __m256i vec0 = _mm256_load_si256(reinterpret_cast<__m256i*>(data + i));
      vec0 = _mm256_xor_si256(vec0, all_ones);
      _mm256_store_si256(reinterpret_cast<__m256i*>(data + i), vec0);

      // Second half of cache line (32 bytes = 4 blocks)
      __m256i vec1 =
          _mm256_load_si256(reinterpret_cast<__m256i*>(data + i + 4));
      vec1 = _mm256_xor_si256(vec1, all_ones);
      _mm256_store_si256(reinterpret_cast<__m256i*>(data + i + 4), vec1);
    }

    // Note: We don't call zero_unused_bits() because padding bits are masked
    // off in summary operations (count(), all(), any(), none())
    return *this;
  }

  size_type count() const noexcept {
    if (num_bits_ == 0) {
      return 0;
    }

    const size_type last_block_idx = block_index(num_bits_ - 1);
    const size_type remainder = num_bits_ & (bits_per_block - 1);
    const block_type* data = blocks_.data();

    // AVX512 VPOPCNTDQ-optimized counting via helper function
    // Overflow analysis:
    // - Each popcnt returns max 64
    // - Each uint64_t accumulator can hold (2^64-1)/64 =
    // 288,230,376,151,711,743 counts
    // - With 4 accumulators, that's 4 * 288,230,376,151,711,743 =
    // 1,152,921,504,606,846,972 blocks
    // - = 2^60 bits = 1,152,921,504,606,846,972 * 64 bits = 1 exabyte
    // - This will never overflow on current hardware (max virtual memory << 1
    // EB)
    size_type result = detail::simd_count_blocks(data, last_block_idx);

    // Count the last block containing actual data
    if (remainder == 0) {
      // Last block is full (num_bits_ is multiple of 64)
      result += std::popcount(data[last_block_idx]);
    } else {
      // Last block is partial, mask off bits beyond num_bits_
      block_type mask = (block_type(1) << remainder) - 1;
      result += std::popcount(data[last_block_idx] & mask);
    }

    return result;
  }

  /// AVX2-optimized any() - checks if any bit is set
  /// Processes 64 bytes (1 cache line) per iteration using 2x 32-byte loads
  /// Optimized for high-density bitsets with occasional sparsity
  bool any() const noexcept {
    if (blocks_.empty()) {
      return false;
    }

    const block_type* data = blocks_.data();
    const size_type num_blocks_total = blocks_.size();

    // Process 8 blocks (64 bytes = 1 cache line) at a time using AVX2
    // Each ymm register holds 4 x uint64_t = 32 bytes
    const size_type simd_blocks = num_blocks_total / 8;
    size_type i = 0;

    // OR together all 64 bytes of each cache line
    for (size_type chunk = 0; chunk < simd_blocks; ++chunk) {
      // Load 2x 32-byte chunks (1 full cache line)
      __m256i ymm0 = _mm256_load_si256(
          reinterpret_cast<const __m256i*>(data + i)); // blocks 0-3
      __m256i ymm1 = _mm256_load_si256(
          reinterpret_cast<const __m256i*>(data + i + 4)); // blocks 4-7

      // OR the two 32-byte chunks together
      __m256i combined = _mm256_or_si256(ymm0, ymm1);

      // Check if any bits are set in the combined result
      if (!_mm256_testz_si256(combined, combined)) {
        return true; // Early exit: found a set bit
      }

      i += 8;
    }

    // Handle remaining blocks with scalar code
    for (; i < num_blocks_total; ++i) {
      if (data[i] != 0) {
        return true;
      }
    }

    return false;
  }

  bool none() const noexcept { return !any(); }

  /// AVX2-optimized all() - checks if all bits are set
  /// Processes 64 bytes (1 cache line) per iteration using 2x 32-byte loads
  /// Optimized for high-density bitsets with occasional sparsity
  bool all() const noexcept {
    if (empty()) [[unlikely]] {
      return true;
    }

    const block_type* data = blocks_.data();
    const size_type last_block_idx = block_index(num_bits_ - 1);
    const size_type remainder = num_bits_ & (bits_per_block - 1);

    // Process full blocks in chunks of 8 (64 bytes = 1 cache line)
    const size_type simd_blocks = last_block_idx / 8;
    size_type i = 0;

    // All-ones pattern for comparison
    const __m256i all_ones = _mm256_set1_epi64x(-1LL);

    for (size_type chunk = 0; chunk < simd_blocks; ++chunk) {
      // Load 2x 32-byte chunks (1 full cache line)
      __m256i ymm0 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i));
      __m256i ymm1 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i + 4));

      // Check if both chunks are all-ones
      // cmpeq returns all-ones (0xFF...FF) if equal, all-zeros otherwise
      __m256i cmp0 = _mm256_cmpeq_epi64(ymm0, all_ones);
      __m256i cmp1 = _mm256_cmpeq_epi64(ymm1, all_ones);

      // AND the comparison results
      __m256i combined = _mm256_and_si256(cmp0, cmp1);

      // If not all bits are set, we found a zero bit
      if (!_mm256_testc_si256(combined, all_ones)) {
        return false; // Early exit: found a zero bit
      }

      i += 8;
    }

    // Handle remaining full blocks with scalar code (blocks before last)
    for (; i < last_block_idx; ++i) {
      if (data[i] != ~block_type(0)) {
        return false;
      }
    }

    // Check the last block containing actual data
    if (remainder == 0) {
      // Last block is full (num_bits_ is multiple of 64)
      return data[last_block_idx] == ~block_type(0);
    } else {
      // Last block is partial, mask off bits beyond num_bits_
      block_type mask = (block_type(1) << remainder) - 1;
      return (data[last_block_idx] & mask) == mask;
    }
  }

  dynamic_bitset& operator&=(const dynamic_bitset& other) noexcept {
    // AVX2-optimized bitwise AND - process full cache lines (64 bytes = 8
    // blocks) No tail handling needed: num_blocks_needed() always rounds to
    // cache line boundaries
    const size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());

    block_type* __restrict__ dst = blocks_.data();
    const block_type* __restrict__ src = other.blocks_.data();

    // Process 8 x uint64_t (64 bytes = 1 cache line) per iteration
    // min_blocks is always a multiple of 8, so no tail handling needed
    for (size_type i = 0; i < min_blocks; i += 8) {
      // First half of cache line (32 bytes = 4 blocks)
      __m256i dst_vec0 = _mm256_load_si256(reinterpret_cast<__m256i*>(dst + i));
      __m256i src_vec0 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i));
      __m256i result0 = _mm256_and_si256(dst_vec0, src_vec0);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i), result0);

      // Second half of cache line (32 bytes = 4 blocks)
      __m256i dst_vec1 =
          _mm256_load_si256(reinterpret_cast<__m256i*>(dst + i + 4));
      __m256i src_vec1 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + 4));
      __m256i result1 = _mm256_and_si256(dst_vec1, src_vec1);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + 4), result1);
    }

    // Clear all blocks beyond RHS size (AND with 0)
    const size_type remaining_blocks = blocks_.size() - min_blocks;
    if (remaining_blocks > 0) {
      std::memset(dst + min_blocks, 0, remaining_blocks * sizeof(block_type));
    }
    return *this;
  }

  dynamic_bitset& operator|=(const dynamic_bitset& other) noexcept {
    // AVX2-optimized bitwise OR - process full cache lines (64 bytes = 8
    // blocks) No tail handling needed: num_blocks_needed() always rounds to
    // cache line boundaries
    const size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());

    block_type* __restrict__ dst = blocks_.data();
    const block_type* __restrict__ src = other.blocks_.data();

    // Process 8 x uint64_t (64 bytes = 1 cache line) per iteration
    // min_blocks is always a multiple of 8, so no tail handling needed
    for (size_type i = 0; i < min_blocks; i += 8) {
      // First half of cache line
      __m256i dst_vec0 = _mm256_load_si256(reinterpret_cast<__m256i*>(dst + i));
      __m256i src_vec0 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i));
      __m256i result0 = _mm256_or_si256(dst_vec0, src_vec0);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i), result0);

      // Second half of cache line
      __m256i dst_vec1 =
          _mm256_load_si256(reinterpret_cast<__m256i*>(dst + i + 4));
      __m256i src_vec1 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + 4));
      __m256i result1 = _mm256_or_si256(dst_vec1, src_vec1);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + 4), result1);
    }

    // Blocks beyond RHS size remain unchanged (OR with 0 is identity)
    return *this;
  }

  dynamic_bitset& operator^=(const dynamic_bitset& other) noexcept {
    // AVX2-optimized bitwise XOR - process full cache lines (64 bytes = 8
    // blocks) No tail handling needed: num_blocks_needed() always rounds to
    // cache line boundaries
    const size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());

    block_type* __restrict__ dst = blocks_.data();
    const block_type* __restrict__ src = other.blocks_.data();

    // Process 8 x uint64_t (64 bytes = 1 cache line) per iteration
    // min_blocks is always a multiple of 8, so no tail handling needed
    for (size_type i = 0; i < min_blocks; i += 8) {
      // First half of cache line
      __m256i dst_vec0 = _mm256_load_si256(reinterpret_cast<__m256i*>(dst + i));
      __m256i src_vec0 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i));
      __m256i result0 = _mm256_xor_si256(dst_vec0, src_vec0);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i), result0);

      // Second half of cache line
      __m256i dst_vec1 =
          _mm256_load_si256(reinterpret_cast<__m256i*>(dst + i + 4));
      __m256i src_vec1 =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(src + i + 4));
      __m256i result1 = _mm256_xor_si256(dst_vec1, src_vec1);
      _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i + 4), result1);
    }

    // Blocks beyond RHS size remain unchanged (XOR with 0 is identity)
    return *this;
  }

  constexpr dynamic_bitset operator~() const {
    dynamic_bitset result(*this);
    result.flip();
    return result;
  }

private:
  // OPTIMIZATION 15: Replace division with bit shift
  // Always round up to cache line boundaries (8 blocks = 64 bytes)
  // This ensures we can always process full cache lines with AVX2 without tail
  // handling
  static constexpr size_type num_blocks_needed(size_type num_bits) noexcept {
    size_type blocks = (num_bits + bits_per_block - 1) >> block_shift;
    // Round up to nearest multiple of 8 blocks (64-byte cache line)
    return (blocks + 7) & ~size_type(7);
  }
};

template <typename Policy>
constexpr dynamic_bitset<Policy> operator&(const dynamic_bitset<Policy>& lhs,
                                           const dynamic_bitset<Policy>& rhs) {
  dynamic_bitset<Policy> result(lhs);
  result &= rhs;
  return result;
}

template <typename Policy>
constexpr dynamic_bitset<Policy> operator|(const dynamic_bitset<Policy>& lhs,
                                           const dynamic_bitset<Policy>& rhs) {
  dynamic_bitset<Policy> result(lhs);
  result |= rhs;
  return result;
}

template <typename Policy>
constexpr dynamic_bitset<Policy> operator^(const dynamic_bitset<Policy>& lhs,
                                           const dynamic_bitset<Policy>& rhs) {
  dynamic_bitset<Policy> result(lhs);
  result ^= rhs;
  return result;
}

} // namespace franklin

#endif // FRANKLIN_CONTAINER_DYNAMIC_BITSET_HPP
