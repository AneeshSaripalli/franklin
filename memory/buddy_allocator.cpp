#include "memory/buddy_allocator.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace franklin {

// Calculate log2 of a power-of-2 value
static inline std::size_t log2_pow2(std::size_t x) {
  return std::bit_width(x) - 1;
}

// Round up to nearest power of 2
static inline std::size_t next_pow2(std::size_t x) {
  if (x <= 1)
    return 1;
  return std::size_t(1) << std::bit_width(x - 1);
}

buddy_allocator::buddy_allocator(std::size_t pool_size, void* pool_ptr)
    : pool_ptr_(static_cast<std::byte*>(pool_ptr)), pool_size_(pool_size),
      owns_memory_(pool_ptr == nullptr) {

  // Validate pool size
  FRANKLIN_ASSERT(pool_size >= MIN_BLOCK_SIZE &&
                  "Pool size must be at least MIN_BLOCK_SIZE");
  FRANKLIN_ASSERT((pool_size & (pool_size - 1)) == 0 &&
                  "Pool size must be a power of 2");

  // Calculate number of levels
  // Level 0 = entire pool, Level N = MIN_BLOCK_SIZE
  num_levels_ = log2_pow2(pool_size_ / MIN_BLOCK_SIZE) + 1;

  // Allocate memory if needed
  if (owns_memory_) {
    // Use aligned_alloc for cache line alignment
    pool_ptr_ =
        static_cast<std::byte*>(std::aligned_alloc(MIN_BLOCK_SIZE, pool_size_));
    FRANKLIN_ASSERT(pool_ptr_ != nullptr && "Failed to allocate pool memory");
  }

  // Calculate total number of blocks across all levels
  // Level 0: 1 block, Level 1: 2 blocks, ..., Level N: 2^N blocks
  // Total = 2^0 + 2^1 + ... + 2^N = 2^(N+1) - 1
  std::size_t total_blocks = (std::size_t(1) << num_levels_) - 1;
  allocation_map_.resize(total_blocks);

  // Initialize free lists
  free_lists_.resize(num_levels_);

  // Initially, only the largest block (level 0, index 0) is free
  free_lists_[0].push_back(0);
}

buddy_allocator::~buddy_allocator() {
  if (owns_memory_ && pool_ptr_ != nullptr) {
    std::free(pool_ptr_);
  }
}

std::size_t buddy_allocator::size_to_level(std::size_t size) const {
  // Round size up to next power of 2
  std::size_t rounded = next_pow2(size);
  if (rounded < MIN_BLOCK_SIZE) {
    rounded = MIN_BLOCK_SIZE;
  }

  // Calculate level
  // Level 0 = pool_size_, Level N = MIN_BLOCK_SIZE
  std::size_t level = log2_pow2(pool_size_ / rounded);

  FRANKLIN_DEBUG_ASSERT(level < num_levels_);
  return level;
}

std::size_t buddy_allocator::level_to_block_size(std::size_t level) const {
  return pool_size_ >> level;
}

std::size_t buddy_allocator::get_bitmap_index(std::size_t level,
                                              std::size_t index) const {
  // Bitmap layout: [Level 0: 1 block][Level 1: 2 blocks][Level 2: 4 blocks]...
  // Offset for level L is (2^L - 1)
  std::size_t offset = (std::size_t(1) << level) - 1;
  return offset + index;
}

std::size_t buddy_allocator::get_buddy_index(std::size_t index) const {
  // Buddy is obtained by flipping the least significant bit
  return index ^ 1;
}

void buddy_allocator::split_block(std::size_t level, std::size_t index) {
  FRANKLIN_DEBUG_ASSERT(level + 1 < num_levels_);

  // Mark the parent block as allocated (split)
  std::size_t bitmap_idx = get_bitmap_index(level, index);
  allocation_map_.set(bitmap_idx);

  // Add the two child blocks to the next level's free list
  std::size_t child_level = level + 1;
  std::size_t left_child = index * 2;
  std::size_t right_child = index * 2 + 1;

  free_lists_[child_level].push_back(left_child);
  free_lists_[child_level].push_back(right_child);
}

void buddy_allocator::merge_block(std::size_t level, std::size_t index) {
  if (level == 0) {
    return; // Can't merge at the top level
  }

  std::size_t buddy_idx = get_buddy_index(index);

  // Check if buddy is free
  std::size_t buddy_bitmap_idx = get_bitmap_index(level, buddy_idx);
  if (allocation_map_.test(buddy_bitmap_idx)) {
    return; // Buddy is allocated, can't merge
  }

  // Both this block and buddy are free, merge them
  // Remove both from the current level's free list
  auto& free_list = free_lists_[level];
  auto it = std::find(free_list.begin(), free_list.end(), index);
  if (it != free_list.end()) {
    free_list.erase(it);
  }
  it = std::find(free_list.begin(), free_list.end(), buddy_idx);
  if (it != free_list.end()) {
    free_list.erase(it);
  }

  // Clear both blocks in the bitmap
  allocation_map_.reset(get_bitmap_index(level, index));
  allocation_map_.reset(buddy_bitmap_idx);

  // Add the parent block to the previous level's free list
  std::size_t parent_level = level - 1;
  std::size_t parent_index = index / 2;
  free_lists_[parent_level].push_back(parent_index);

  // Clear the parent's allocation bit
  allocation_map_.reset(get_bitmap_index(parent_level, parent_index));

  // Try to merge recursively
  merge_block(parent_level, parent_index);
}

void* buddy_allocator::get_block_ptr(std::size_t level,
                                     std::size_t index) const {
  std::size_t block_size = level_to_block_size(level);
  std::size_t offset = index * block_size;
  return pool_ptr_ + offset;
}

void buddy_allocator::ptr_to_block(void* ptr, std::size_t& level,
                                   std::size_t& index) const {
  FRANKLIN_DEBUG_ASSERT(ptr >= pool_ptr_ && ptr < pool_ptr_ + pool_size_);

  std::size_t offset = static_cast<std::byte*>(ptr) - pool_ptr_;

  // Find the level by looking at which block contains this pointer
  // Start from the smallest level and work up
  for (std::size_t l = num_levels_ - 1; l < num_levels_; --l) {
    std::size_t block_size = level_to_block_size(l);
    std::size_t idx = offset / block_size;
    std::size_t bitmap_idx = get_bitmap_index(l, idx);

    if (allocation_map_.test(bitmap_idx)) {
      level = l;
      index = idx;
      return;
    }
  }

  FRANKLIN_ASSERT(false && "Invalid pointer passed to deallocate");
}

void* buddy_allocator::allocate(std::size_t size) {
  if (size == 0 || size > pool_size_) {
    return nullptr;
  }

  // Find the appropriate level for this size
  std::size_t target_level = size_to_level(size);

  // Find the first level with a free block, starting from target_level
  std::size_t alloc_level = target_level;
  while (alloc_level < num_levels_ && free_lists_[alloc_level].empty()) {
    ++alloc_level;
  }

  if (alloc_level >= num_levels_) {
    return nullptr; // No free blocks available
  }

  // Split blocks until we reach the target level
  while (alloc_level > target_level) {
    // Take a block from the current level
    std::size_t block_index = free_lists_[alloc_level].back();
    free_lists_[alloc_level].pop_back();

    // Split it
    split_block(alloc_level, block_index);
    --alloc_level;
  }

  // Allocate the block at target_level
  std::size_t block_index = free_lists_[target_level].back();
  free_lists_[target_level].pop_back();

  // Mark as allocated
  std::size_t bitmap_idx = get_bitmap_index(target_level, block_index);
  allocation_map_.set(bitmap_idx);

  return get_block_ptr(target_level, block_index);
}

void buddy_allocator::deallocate(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  // Find which block this pointer corresponds to
  std::size_t level, index;
  ptr_to_block(ptr, level, index);

  // Mark as free
  std::size_t bitmap_idx = get_bitmap_index(level, index);
  allocation_map_.reset(bitmap_idx);

  // Add to free list
  free_lists_[level].push_back(index);

  // Try to merge with buddy
  merge_block(level, index);
}

} // namespace franklin
