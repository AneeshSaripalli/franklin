#include "core/interpreter.hpp"
#include <iostream>

int main() {
  using namespace franklin;

  // Create an interpreter (non-templated, supports heterogeneous types)
  interpreter interp;

  // Create some column vectors using default policies
  column_vector<Int32DefaultPolicy> col1(5, 10); // 5 elements, all set to 10
  column_vector<Int32DefaultPolicy> col2(5, 20); // 5 elements, all set to 20

  // Register them with names (takes ownership via move)
  interp.register_column("a", std::move(col1));
  interp.register_column("b", std::move(col2));

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
