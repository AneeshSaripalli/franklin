#ifndef FRANKLIN_CONTAINER_DYNAMIC_BITSET_HPP
#define FRANKLIN_CONTAINER_DYNAMIC_BITSET_HPP

#include "core/error_collector.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

namespace franklin {

template <typename Policy> class dynamic_bitset {
public:
  using block_type = std::uint64_t;
  using size_type = typename Policy::size_type;

  static constexpr size_type bits_per_block = 64;
  static constexpr size_type block_shift = 6; // log2(64) = 6

private:
  std::vector<block_type> blocks_;
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
    if (__builtin_expect(pos >= num_bits_, 0)) {
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

  constexpr bool operator[](size_type pos) const noexcept {
    return (blocks_[block_index(pos)] & bit_mask(pos)) != 0;
  }

  constexpr dynamic_bitset& set(size_type pos, bool value = true) {
    // OPTIMIZATION 7: Branch prediction hint - bounds check rarely fails
    if (__builtin_expect(pos >= num_bits_, 0)) {
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
    if (__builtin_expect(value, 1)) {
      blocks_[block_index(pos)] |= bit_mask(pos);
    } else {
      blocks_[block_index(pos)] &= ~bit_mask(pos);
    }
    return *this;
  }

  constexpr dynamic_bitset& set() noexcept {
    for (auto& block : blocks_) {
      block = ~block_type(0);
    }
    zero_unused_bits();
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
    if (__builtin_expect(pos >= num_bits_, 0)) {
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

  constexpr dynamic_bitset& flip() noexcept {
    for (auto& block : blocks_) {
      block = ~block;
    }
    zero_unused_bits();
    return *this;
  }

  constexpr size_type count() const noexcept {
    size_type result = 0;
    for (const auto& block : blocks_) {
      result += __builtin_popcountll(block);
    }
    return result;
  }

  constexpr bool all() const noexcept {
    // OPTIMIZATION 11: Early exit and bit manipulation instead of modulo
    if (__builtin_expect(empty(), 0))
      return true;

    const size_type num_full_blocks = blocks_.size() - 1;

    // Check all full blocks
    for (size_type i = 0; i < num_full_blocks; ++i) {
      if (__builtin_expect(blocks_[i] != ~block_type(0), 0)) {
        return false;
      }
    }

    // Check last block with bit mask instead of modulo
    const size_type remainder = num_bits_ & (bits_per_block - 1);
    if (remainder == 0) {
      return blocks_.back() == ~block_type(0);
    } else {
      block_type mask = (block_type(1) << remainder) - 1;
      return (blocks_.back() & mask) == mask;
    }
  }

  constexpr bool any() const noexcept {
    for (const auto& block : blocks_) {
      if (block != 0) {
        return true;
      }
    }
    return false;
  }

  constexpr bool none() const noexcept { return !any(); }

  constexpr dynamic_bitset& operator&=(const dynamic_bitset& other) noexcept {
    // OPTIMIZATION 12: Use pointer iteration for better codegen
    const size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());

    block_type* __restrict__ dst = blocks_.data();
    const block_type* __restrict__ src = other.blocks_.data();

    // AND the overlapping portion
    for (size_type i = 0; i < min_blocks; ++i) {
      dst[i] &= src[i];
    }

    // Fix Bug #4: Clear all blocks beyond RHS size (AND with 0) using memset
    const size_type remaining_blocks = blocks_.size() - min_blocks;
    if (remaining_blocks > 0) {
      std::memset(dst + min_blocks, 0, remaining_blocks * sizeof(block_type));
    }
    return *this;
  }

  constexpr dynamic_bitset& operator|=(const dynamic_bitset& other) noexcept {
    // OPTIMIZATION 13: Use pointer iteration for better codegen
    const size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());

    block_type* __restrict__ dst = blocks_.data();
    const block_type* __restrict__ src = other.blocks_.data();

    for (size_type i = 0; i < min_blocks; ++i) {
      dst[i] |= src[i];
    }
    // Fix Bug #4: Blocks beyond RHS size remain unchanged (OR with 0 is
    // identity)
    return *this;
  }

  constexpr dynamic_bitset& operator^=(const dynamic_bitset& other) noexcept {
    // OPTIMIZATION 14: Use pointer iteration for better codegen
    const size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());

    block_type* __restrict__ dst = blocks_.data();
    const block_type* __restrict__ src = other.blocks_.data();

    for (size_type i = 0; i < min_blocks; ++i) {
      dst[i] ^= src[i];
    }
    // Fix Bug #4: Blocks beyond RHS size remain unchanged (XOR with 0 is
    // identity)
    return *this;
  }

  constexpr dynamic_bitset operator~() const {
    dynamic_bitset result(*this);
    result.flip();
    return result;
  }

private:
  // OPTIMIZATION 15: Replace division with bit shift
  static constexpr size_type num_blocks_needed(size_type num_bits) noexcept {
    return (num_bits + bits_per_block - 1) >> block_shift;
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
