#ifndef FRANKLIN_MEMORY_BUDDY_ALLOCATOR_HPP
#define FRANKLIN_MEMORY_BUDDY_ALLOCATOR_HPP

#include "container/dynamic_bitset.hpp"
#include "core/compiler_macros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace franklin {

// Default policy for dynamic_bitset used in buddy_allocator
struct bitset_default_policy {
  using size_type = std::size_t;
};

// Buddy allocator that uses a bitmap to track allocation status.
// Minimum allocation size is FRANKLIN_CACHE_LINE_SIZE (64 bytes).
// All allocations are rounded up to the nearest power of 2.
class buddy_allocator {
public:
  static constexpr std::size_t MIN_BLOCK_SIZE = FRANKLIN_CACHE_LINE_SIZE;

  // Construct a buddy allocator managing a memory pool of the given size.
  // pool_size must be a power of 2 and >= MIN_BLOCK_SIZE.
  // If pool_ptr is nullptr, the allocator will allocate its own memory.
  explicit buddy_allocator(std::size_t pool_size, void* pool_ptr = nullptr);

  ~buddy_allocator();

  // Allocate a block of at least size bytes.
  // Returns nullptr if allocation fails.
  void* allocate(std::size_t size);

  // Deallocate a previously allocated block.
  void deallocate(void* ptr);

  // Get the size of the pool managed by this allocator.
  std::size_t pool_size() const { return pool_size_; }

  // Get the number of levels in the buddy system.
  std::size_t num_levels() const { return num_levels_; }

private:
  // Calculate which level a size belongs to (0 = largest blocks)
  std::size_t size_to_level(std::size_t size) const;

  // Calculate the block size at a given level
  std::size_t level_to_block_size(std::size_t level) const;

  // Get the bitmap index for a block at (level, index)
  std::size_t get_bitmap_index(std::size_t level, std::size_t index) const;

  // Get the buddy index for a block index at a given level
  std::size_t get_buddy_index(std::size_t index) const;

  // Split a block at the given level and index
  void split_block(std::size_t level, std::size_t index);

  // Try to merge a block with its buddy
  void merge_block(std::size_t level, std::size_t index);

  // Get the pointer for a block at (level, index)
  void* get_block_ptr(std::size_t level, std::size_t index) const;

  // Get the (level, index) for a given pointer
  void ptr_to_block(void* ptr, std::size_t& level, std::size_t& index) const;

  std::byte* pool_ptr_;    // Base pointer to memory pool
  std::size_t pool_size_;  // Total size of the pool
  std::size_t num_levels_; // Number of levels in the buddy system
  bool owns_memory_;       // Whether we allocated the pool memory
  dynamic_bitset<bitset_default_policy>
      allocation_map_; // Bitmap: 0 = free, 1 = allocated

  // Free lists for each level (indices of free blocks)
  std::vector<std::vector<std::size_t>> free_lists_;
};

} // namespace franklin

#endif // FRANKLIN_MEMORY_BUDDY_ALLOCATOR_HPP
