#ifndef FRANKLIN_CORE_DYNAMIC_BITSET_HPP
#define FRANKLIN_CORE_DYNAMIC_BITSET_HPP

#include "error_collector.hpp"
#include <cstdint>
#include <vector>

namespace franklin {

template <typename Policy> class dynamic_bitset {
public:
  using block_type = std::uint64_t;
  using size_type = typename Policy::size_type;

  static constexpr size_type bits_per_block = 64;

private:
  std::vector<block_type> blocks_;
  size_type num_bits_;

  static constexpr size_type block_index(size_type pos) noexcept {
    return pos / bits_per_block;
  }

  static constexpr size_type bit_index(size_type pos) noexcept {
    return pos % bits_per_block;
  }

  static constexpr block_type bit_mask(size_type pos) noexcept {
    return block_type(1) << bit_index(pos);
  }

  constexpr size_type num_blocks() const noexcept {
    return (num_bits_ + bits_per_block - 1) / bits_per_block;
  }

  constexpr void zero_unused_bits() noexcept {
    if (num_bits_ % bits_per_block != 0) {
      size_type used_bits = num_bits_ % bits_per_block;
      block_type mask = (block_type(1) << used_bits) - 1;
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
    if (pos >= num_bits_) {
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
    if (pos >= num_bits_) {
      auto error =
          core::make_error(core::ErrorCode::OutOfRange, "dynamic_bitset", "set",
                           "Position exceeds bitset size");
      error.add_context(pos);
      error.add_context(num_bits_);
      error.add_context(value);
      core::report_error(std::move(error));
      return *this;
    }

    if (value) {
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
    for (auto& block : blocks_) {
      block = block_type(0);
    }
    return *this;
  }

  constexpr dynamic_bitset& flip(size_type pos) {
    if (pos >= num_bits_) {
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
    if (empty())
      return true;

    for (size_type i = 0; i < blocks_.size() - 1; ++i) {
      if (blocks_[i] != ~block_type(0)) {
        return false;
      }
    }

    if (num_bits_ % bits_per_block == 0) {
      return blocks_.back() == ~block_type(0);
    } else {
      size_type used_bits = num_bits_ % bits_per_block;
      block_type mask = (block_type(1) << used_bits) - 1;
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
    size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());
    for (size_type i = 0; i < min_blocks; ++i) {
      blocks_[i] &= other.blocks_[i];
    }
    // Fix Bug #4: Clear all blocks beyond RHS size (AND with 0)
    for (size_type i = min_blocks; i < blocks_.size(); ++i) {
      blocks_[i] = 0;
    }
    return *this;
  }

  constexpr dynamic_bitset& operator|=(const dynamic_bitset& other) noexcept {
    size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());
    for (size_type i = 0; i < min_blocks; ++i) {
      blocks_[i] |= other.blocks_[i];
    }
    // Fix Bug #4: Blocks beyond RHS size remain unchanged (OR with 0 is
    // identity) No action needed for blocks_[i] where i >= min_blocks
    return *this;
  }

  constexpr dynamic_bitset& operator^=(const dynamic_bitset& other) noexcept {
    size_type min_blocks = std::min(blocks_.size(), other.blocks_.size());
    for (size_type i = 0; i < min_blocks; ++i) {
      blocks_[i] ^= other.blocks_[i];
    }
    // Fix Bug #4: Blocks beyond RHS size remain unchanged (XOR with 0 is
    // identity) No action needed for blocks_[i] where i >= min_blocks
    return *this;
  }

  constexpr dynamic_bitset operator~() const {
    dynamic_bitset result(*this);
    result.flip();
    return result;
  }

private:
  static constexpr size_type num_blocks_needed(size_type num_bits) noexcept {
    return (num_bits + bits_per_block - 1) / bits_per_block;
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

#endif // FRANKLIN_CORE_DYNAMIC_BITSET_HPP
