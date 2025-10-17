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

  std::cout << "Test: Constructor with size - present() should return true\n";
  Int32Column col(10);

  std::cout << "Calling col.present(0)...\n";
  bool result0 = col.present(0);
  std::cout << "col.present(0) = " << result0 << " (expected: true)\n";
  std::cout << "RESULT: " << (result0 ? "PASS" : "FAIL") << "\n\n";

  std::cout << "Calling col.present(5)...\n";
  bool result5 = col.present(5);
  std::cout << "col.present(5) = " << result5 << " (expected: true)\n";
  std::cout << "RESULT: " << (result5 ? "PASS" : "FAIL") << "\n\n";

  std::cout << "Calling col.present(9)...\n";
  bool result9 = col.present(9);
  std::cout << "col.present(9) = " << result9 << " (expected: true)\n";
  std::cout << "RESULT: " << (result9 ? "PASS" : "FAIL") << "\n";

  return 0;
}
