#include "container/column.hpp"
#include <cstdint>
#include <iostream>

namespace franklin {

struct Int32ColumnPolicy {
  using value_type = std::int32_t;
  using allocator_type = std::allocator<std::int32_t>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = false;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = true;
};

using Int32Column = column_vector<Int32ColumnPolicy>;

} // namespace franklin

int main() {
  using namespace franklin;

  std::cout << "Test 1: Constructor with size - present() should return true\n";
  {
    Int32Column col(10);
    bool result = col.present(0);
    std::cout << "  col.present(0) = " << result << " (expected: true)\n";
    std::cout << "  RESULT: " << (result ? "PASS" : "FAIL") << "\n\n";
  }

  std::cout << "Test 2: Out of bounds access\n";
  {
    Int32Column col(10);
    std::cout << "  Attempting col.present(100)...\n";
    bool result = col.present(100);
    std::cout << "  col.present(100) = " << result << " (undefined behavior)\n";
    std::cout << "  If you see this, UB didn't crash (yet)\n\n";
  }

  std::cout << "Test 3: Empty vector out of bounds\n";
  {
    Int32Column col(0);
    std::cout << "  Attempting col.present(0) on empty vector...\n";
    bool result = col.present(0);
    std::cout << "  col.present(0) = " << result << " (undefined behavior)\n";
    std::cout << "  If you see this, UB didn't crash (yet)\n\n";
  }

  std::cout << "Test 4: Stale data after copy assignment\n";
  {
    Int32Column col1(10, 42);
    Int32Column col2(5, 99);

    bool before = col1.present(9);
    std::cout << "  Before assignment: col1.present(9) = " << before << "\n";

    col1 = col2;

    std::cout
        << "  After col1 = col2 (size 5), attempting col1.present(9)...\n";
    bool after = col1.present(9);
    std::cout << "  col1.present(9) = " << after
              << " (should be false or UB)\n\n";
  }

  std::cout << "Test 5: Very large out of bounds index\n";
  {
    Int32Column col(1, 42);
    std::cout << "  Attempting col.present(1000) on size-1 vector...\n";
    volatile bool r = col.present(1000);
    std::cout << "  col.present(1000) = " << r << " (undefined behavior)\n";
    std::cout << "  If you see this, severe UB didn't crash (yet)\n\n";
  }

  std::cout << "All tests completed (but some had UB)\n";
  return 0;
}
