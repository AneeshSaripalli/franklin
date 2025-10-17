#ifndef FRANKLIN_BENCHMARKS_BITSET_WRAPPER_HPP
#define FRANKLIN_BENCHMARKS_BITSET_WRAPPER_HPP

#include "container/dynamic_bitset.hpp"
#include "container/dynamic_bitset_optimized.hpp"

#include <cstddef>

struct BenchmarkPolicy {
  using size_type = std::size_t;
};

// Original version (from dynamic_bitset.hpp)
template <typename Policy>
using BitsetOriginal = franklin::dynamic_bitset<Policy>;

// Create a wrapper for the optimized version
namespace franklin_opt {
template <typename Policy> class dynamic_bitset_optimized;
}

template <typename Policy>
using BitsetOptimized = franklin::dynamic_bitset<Policy>;

#endif // FRANKLIN_BENCHMARKS_BITSET_WRAPPER_HPP
