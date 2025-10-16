#include "memory/buddy_allocator.hpp"
#include "memory/buddy_allocator_adapter.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace franklin {

// ============================================================================
// Basic Allocator Adapter Tests
// ============================================================================

TEST(BuddyAllocatorAdapterTest, BasicAllocation) {
  // Create 1MB pool
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc(&pool);

  // Allocate 10 ints
  int* ptr = alloc.allocate(10);
  ASSERT_NE(ptr, nullptr);

  // Write and read values
  for (int i = 0; i < 10; ++i) {
    ptr[i] = i * 2;
  }

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(ptr[i], i * 2);
  }

  // Deallocate
  alloc.deallocate(ptr, 10);
}

TEST(BuddyAllocatorAdapterTest, ZeroSizeAllocation) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc(&pool);

  // Allocate 0 elements
  int* ptr = alloc.allocate(0);
  EXPECT_EQ(ptr, nullptr);

  // Should be safe to deallocate nullptr
  alloc.deallocate(ptr, 0);
}

TEST(BuddyAllocatorAdapterTest, MultipleAllocations) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc(&pool);

  // Allocate multiple blocks
  int* ptr1 = alloc.allocate(10);
  int* ptr2 = alloc.allocate(20);
  int* ptr3 = alloc.allocate(30);

  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);
  ASSERT_NE(ptr3, nullptr);

  // Pointers should be different
  EXPECT_NE(ptr1, ptr2);
  EXPECT_NE(ptr2, ptr3);
  EXPECT_NE(ptr1, ptr3);

  // Deallocate in different order
  alloc.deallocate(ptr2, 20);
  alloc.deallocate(ptr1, 10);
  alloc.deallocate(ptr3, 30);
}

// ============================================================================
// STL Container Integration Tests
// ============================================================================

TEST(BuddyAllocatorAdapterTest, VectorIntegration) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc(&pool);

  // Create vector with buddy allocator
  std::vector<int, buddy_allocator_adapter<int>> vec(alloc);

  // Push elements
  for (int i = 0; i < 100; ++i) {
    vec.push_back(i);
  }

  EXPECT_EQ(vec.size(), 100);

  // Verify contents
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(vec[i], i);
  }
}

TEST(BuddyAllocatorAdapterTest, VectorLargeAllocation) {
  buddy_allocator pool(2 * 1024 * 1024); // 2MB pool
  buddy_allocator_adapter<int> alloc(&pool);

  std::vector<int, buddy_allocator_adapter<int>> vec(alloc);

  // Allocate 10000 integers (40KB)
  vec.resize(10000, 42);

  EXPECT_EQ(vec.size(), 10000);
  EXPECT_EQ(vec[0], 42);
  EXPECT_EQ(vec[9999], 42);
}

TEST(BuddyAllocatorAdapterTest, VectorCopy) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc(&pool);

  std::vector<int, buddy_allocator_adapter<int>> vec1(alloc);
  vec1.push_back(1);
  vec1.push_back(2);
  vec1.push_back(3);

  // Copy vector (uses same allocator)
  std::vector<int, buddy_allocator_adapter<int>> vec2 = vec1;

  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);
}

// ============================================================================
// Allocator Requirements Tests
// ============================================================================

TEST(BuddyAllocatorAdapterTest, Rebind) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> int_alloc(&pool);

  // Rebind to double
  using DoubleAlloc = typename std::allocator_traits<
      buddy_allocator_adapter<int>>::template rebind_alloc<double>;

  DoubleAlloc double_alloc(int_alloc);

  // Allocate doubles
  double* ptr = double_alloc.allocate(10);
  ASSERT_NE(ptr, nullptr);

  ptr[0] = 3.14;
  EXPECT_DOUBLE_EQ(ptr[0], 3.14);

  double_alloc.deallocate(ptr, 10);
}

TEST(BuddyAllocatorAdapterTest, EqualityComparison) {
  buddy_allocator pool1(1024 * 1024);
  buddy_allocator pool2(1024 * 1024);

  buddy_allocator_adapter<int> alloc1(&pool1);
  buddy_allocator_adapter<int> alloc2(&pool1); // Same pool
  buddy_allocator_adapter<int> alloc3(&pool2); // Different pool

  // Same pool = equal
  EXPECT_TRUE(alloc1 == alloc2);
  EXPECT_FALSE(alloc1 != alloc2);

  // Different pool = not equal
  EXPECT_FALSE(alloc1 == alloc3);
  EXPECT_TRUE(alloc1 != alloc3);
}

TEST(BuddyAllocatorAdapterTest, MoveConstructor) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc1(&pool);

  // Move construct
  buddy_allocator_adapter<int> alloc2(std::move(alloc1));

  EXPECT_EQ(alloc2.get_allocator(), &pool);
  EXPECT_EQ(alloc1.get_allocator(), nullptr); // Moved-from state
}

TEST(BuddyAllocatorAdapterTest, CopyConstructor) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc1(&pool);

  // Copy construct
  buddy_allocator_adapter<int> alloc2(alloc1);

  EXPECT_EQ(alloc2.get_allocator(), &pool);
  EXPECT_EQ(alloc1.get_allocator(), &pool); // Original unchanged
}

// ============================================================================
// Cache Line Alignment Tests
// ============================================================================

TEST(BuddyAllocatorAdapterTest, AllocationIsAligned) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc(&pool);

  // Allocate various sizes
  for (size_t n : {1, 5, 10, 20, 50, 100}) {
    int* ptr = alloc.allocate(n);
    ASSERT_NE(ptr, nullptr);

    // Check alignment (should be at least 64 bytes = cache line)
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % FRANKLIN_CACHE_LINE_SIZE, 0)
        << "Allocation of " << n << " ints not cache-line aligned";

    alloc.deallocate(ptr, n);
  }
}

TEST(BuddyAllocatorAdapterTest, SafeForSIMDReads) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int32_t> alloc(&pool);

  // Allocate 10 int32_t values (40 bytes)
  // Buddy allocator rounds up to 64 bytes
  int32_t* ptr = alloc.allocate(10);
  ASSERT_NE(ptr, nullptr);

  // Write to allocated space
  for (int i = 0; i < 10; ++i) {
    ptr[i] = i;
  }

  // Reading with AVX2 (32 bytes) should be safe
  // even though we only allocated 10 * 4 = 40 bytes,
  // the buddy allocator gave us 64 bytes
  //
  // This is critical for SIMD vectorization:
  // We can safely load _mm256 registers without ASAN errors

  alloc.deallocate(ptr, 10);
  SUCCEED() << "SIMD-safe allocation verified";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(BuddyAllocatorAdapterTest, OutOfMemoryThrows) {
  // Small pool that will run out quickly
  buddy_allocator pool(1024); // Only 1KB
  buddy_allocator_adapter<int> alloc(&pool);

  // Try to allocate more than pool size
  EXPECT_THROW(
      { int* ptr = alloc.allocate(1000000); }, // 4MB request on 1KB pool
      std::bad_alloc);
}

TEST(BuddyAllocatorAdapterTest, NullptrDeallocateIsSafe) {
  buddy_allocator pool(1024 * 1024);
  buddy_allocator_adapter<int> alloc(&pool);

  // Should not crash
  alloc.deallocate(nullptr, 10);
  SUCCEED();
}

} // namespace franklin

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
