// This file is used for generating assembly output for analysis
// Compile with: clang++ -std=c++20 -O3 -march=native -S -masm=intel

#include "container/dynamic_bitset.hpp"

struct Policy {
  using size_type = std::size_t;
};

using Bitset = franklin::dynamic_bitset<Policy>;

// Force no inlining to see each function separately
__attribute__((noinline)) void test_set_single(Bitset& bs, size_t pos) {
  bs.set(pos, true);
}

__attribute__((noinline)) bool test_get_single(const Bitset& bs, size_t pos) {
  return bs[pos];
}

__attribute__((noinline)) void test_flip_single(Bitset& bs, size_t pos) {
  bs.flip(pos);
}

__attribute__((noinline)) size_t test_count(const Bitset& bs) {
  return bs.count();
}

__attribute__((noinline)) bool test_any(const Bitset& bs) {
  return bs.any();
}

__attribute__((noinline)) bool test_all(const Bitset& bs) {
  return bs.all();
}

__attribute__((noinline)) void test_set_all(Bitset& bs) {
  bs.set();
}

__attribute__((noinline)) void test_reset_all(Bitset& bs) {
  bs.reset();
}

__attribute__((noinline)) void test_flip_all(Bitset& bs) {
  bs.flip();
}

__attribute__((noinline)) void test_bitwise_and(Bitset& lhs,
                                                const Bitset& rhs) {
  lhs &= rhs;
}

__attribute__((noinline)) void test_bitwise_or(Bitset& lhs, const Bitset& rhs) {
  lhs |= rhs;
}

__attribute__((noinline)) void test_bitwise_xor(Bitset& lhs,
                                                const Bitset& rhs) {
  lhs ^= rhs;
}

// Sequential write pattern
__attribute__((noinline)) void test_sequential_write(Bitset& bs) {
  size_t size = bs.size();
  for (size_t i = 0; i < size; ++i) {
    bs.set(i, true);
  }
}

// Sequential read pattern
__attribute__((noinline)) size_t test_sequential_read(const Bitset& bs) {
  size_t size = bs.size();
  size_t count = 0;
  for (size_t i = 0; i < size; ++i) {
    count += bs[i];
  }
  return count;
}

// Block index calculation (hot path)
__attribute__((noinline)) size_t test_block_index(size_t pos) {
  return pos / 64;
}

// Bit index calculation (hot path)
__attribute__((noinline)) size_t test_bit_index(size_t pos) {
  return pos % 64;
}

// Bit mask calculation (hot path)
__attribute__((noinline)) uint64_t test_bit_mask(size_t pos) {
  return uint64_t(1) << (pos % 64);
}
