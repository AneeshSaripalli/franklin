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

  std::cout << "Test: Out of bounds access to index 100 on size-10 vector\n";
  Int32Column col(
      10, 42); // Use the value constructor so present_ is initialized to true

  std::cout << "Calling col.present(100) - this should crash or be caught by "
               "ASAN...\n";
  volatile bool result = col.present(100);
  std::cout << "col.present(100) = " << result
            << " (UNDEFINED BEHAVIOR - if you see this, UB didn't crash yet)\n";

  return 0;
}
