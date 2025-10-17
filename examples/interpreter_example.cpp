#include "container/interpreter.hpp"
#include <iostream>

namespace franklin {

// Define a simple policy for testing
struct ExamplePolicy {
  using value_type = int;
  using allocator_type = std::allocator<int>;
  static constexpr bool is_view = false;
  static constexpr bool allow_missing = false;
  static constexpr bool use_avx512 = false;
  static constexpr bool assume_aligned = false;
};

} // namespace franklin

int main() {
  using namespace franklin;

  // Create an interpreter
  interpreter<ExamplePolicy> interp;

  // Create some column vectors
  column_vector<ExamplePolicy> col1(5, 10); // 5 elements, all set to 10
  column_vector<ExamplePolicy> col2(5, 20); // 5 elements, all set to 20

  // Register them with names
  interp.register_column("a", col1);
  interp.register_column("b", col2);

  std::cout << "Registered columns 'a' and 'b'\n";
  std::cout << "Number of registered columns: " << interp.size() << "\n";
  std::cout << "Has column 'a': " << (interp.has_column("a") ? "yes" : "no")
            << "\n";
  std::cout << "Has column 'c': " << (interp.has_column("c") ? "yes" : "no")
            << "\n";

  // Simple variable lookup works
  try {
    auto result = interp.eval("a");
    std::cout << "Successfully evaluated 'a'\n";
  } catch (const std::exception& e) {
    std::cout << "Error: " << e.what() << "\n";
  }

  // Binary operations not yet implemented
  try {
    auto result = interp.eval("a + b");
    std::cout << "Successfully evaluated 'a + b'\n";
  } catch (const std::exception& e) {
    std::cout << "Expected error (operations not implemented): " << e.what()
              << "\n";
  }

  // Unknown variable
  try {
    auto result = interp.eval("unknown");
    std::cout << "Successfully evaluated 'unknown'\n";
  } catch (const std::exception& e) {
    std::cout << "Expected error (unknown variable): " << e.what() << "\n";
  }

  return 0;
}
