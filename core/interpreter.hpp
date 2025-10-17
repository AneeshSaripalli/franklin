#ifndef FRANKLIN_CORE_INTERPRETER_HPP
#define FRANKLIN_CORE_INTERPRETER_HPP

#include "container/column.hpp"
#include "core/data_type_enum.hpp"
#include <bit>
#include <cctype>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace franklin {

// Type-erased column storage
// Pointer with policy_id packed in top 16 bits (x86-64 canonical addresses use
// lower 48 bits)
struct ErasedColumn {
  std::uintptr_t packed_ptr_and_policy;

  ErasedColumn() : packed_ptr_and_policy(0) {}

  // Pack pointer and policy into a single uintptr_t
  template <concepts::ColumnPolicy Policy>
  explicit ErasedColumn(column_vector<Policy>* ptr) {
    static_assert(sizeof(void*) == 8, "Only 64-bit pointers supported");
    std::uintptr_t ptr_val = std::bit_cast<std::uintptr_t>(ptr);
    std::uintptr_t policy_val = static_cast<std::uintptr_t>(Policy::policy_id);
    // Store policy in top 16 bits
    packed_ptr_and_policy = (ptr_val & 0x0000FFFFFFFFFFFF) | (policy_val << 48);
  }

  // Extract pointer (mask out top 16 bits)
  void* get_ptr() const {
    std::uintptr_t ptr_val = packed_ptr_and_policy & 0x0000FFFFFFFFFFFF;
    return std::bit_cast<void*>(ptr_val);
  }

  // Extract policy from top 16 bits
  DataTypeEnum::Enum get_policy() const {
    return static_cast<DataTypeEnum::Enum>(packed_ptr_and_policy >> 48);
  }

  // Helper to get typed pointer
  template <concepts::ColumnPolicy Policy>
  column_vector<Policy>* get_as() const {
    if (get_policy() != Policy::policy_id) {
      throw std::runtime_error("Type mismatch in get_as");
    }
    return std::bit_cast<column_vector<Policy>*>(get_ptr());
  }
};

// Non-templated interpreter that supports heterogeneous column types
class interpreter {
private:
  std::unordered_map<std::string, ErasedColumn> columns_;

  // Simple tokenizer
  struct Token {
    enum class Type { Variable, Operator, EndOfInput };
    Type type;
    std::string value;
  };

  class Tokenizer {
  private:
    std::string_view input_;
    size_t pos_ = 0;

    void skip_whitespace();

  public:
    explicit Tokenizer(std::string_view input);
    Token next();
  };

  // Parse expression - returns type-erased column
  ErasedColumn parse_expression(Tokenizer& tokenizer);

  // Type-erased arithmetic operations
  static ErasedColumn add(const ErasedColumn& a, const ErasedColumn& b);
  static ErasedColumn subtract(const ErasedColumn& a, const ErasedColumn& b);
  static ErasedColumn multiply(const ErasedColumn& a, const ErasedColumn& b);
  static ErasedColumn bitwise_and(const ErasedColumn& a, const ErasedColumn& b);
  static ErasedColumn bitwise_or(const ErasedColumn& a, const ErasedColumn& b);
  static ErasedColumn bitwise_xor(const ErasedColumn& a, const ErasedColumn& b);

public:
  interpreter() = default;

  ~interpreter() {
    // Clean up all allocated columns
    for (auto& [name, erased] : columns_) {
      delete_erased_column(erased);
    }
  }

  // Delete a type-erased column
  static void delete_erased_column(ErasedColumn erased);

  // Register a column vector with a name (template takes ownership)
  template <concepts::ColumnPolicy Policy>
  void register_column(const std::string& name, column_vector<Policy>&& col) {
    // Check if name exists and delete old column
    auto it = columns_.find(name);
    if (it != columns_.end()) {
      delete_erased_column(it->second);
    }

    // Allocate new column and store
    auto* ptr = new column_vector<Policy>(std::move(col));
    columns_[name] = ErasedColumn(ptr);
  }

  // Evaluate an expression - returns ErasedColumn that caller must delete
  ErasedColumn eval(const std::string& expression);

  // Get a type-erased column by name
  ErasedColumn get_column(const std::string& name) const;

  // Get a typed column by name
  template <concepts::ColumnPolicy Policy>
  const column_vector<Policy>& get_column_typed(const std::string& name) const {
    auto it = columns_.find(name);
    if (it == columns_.end()) {
      throw std::runtime_error("Unknown variable: " + name);
    }
    return *it->second.get_as<Policy>();
  }

  // Check if a name is registered
  bool has_column(const std::string& name) const;

  // Remove a column
  void unregister_column(const std::string& name);

  // Clear all columns
  void clear();

  // Get number of registered columns
  size_t size() const;
};

// ============================================================================
// Implementation
// ============================================================================

// Tokenizer implementation
inline void interpreter::Tokenizer::skip_whitespace() {
  while (pos_ < input_.size() && std::isspace(input_[pos_])) {
    ++pos_;
  }
}

inline interpreter::Tokenizer::Tokenizer(std::string_view input)
    : input_(input) {}

inline interpreter::Token interpreter::Tokenizer::next() {
  skip_whitespace();

  if (pos_ >= input_.size()) {
    return Token{Token::Type::EndOfInput, ""};
  }

  // Check for operators
  char ch = input_[pos_];
  if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '&' ||
      ch == '|' || ch == '^') {
    ++pos_;
    return Token{Token::Type::Operator, std::string(1, ch)};
  }

  // Must be a variable name
  if (std::isalpha(ch) || ch == '_') {
    size_t start = pos_;
    while (pos_ < input_.size() &&
           (std::isalnum(input_[pos_]) || input_[pos_] == '_')) {
      ++pos_;
    }
    return Token{Token::Type::Variable,
                 std::string(input_.substr(start, pos_ - start))};
  }

  throw std::runtime_error("Unexpected character in expression");
}

// Parser implementation
inline ErasedColumn interpreter::parse_expression(Tokenizer& tokenizer) {
  Token first_token = tokenizer.next();

  if (first_token.type != Token::Type::Variable) {
    throw std::runtime_error("Expected variable name");
  }

  auto it = columns_.find(first_token.value);
  if (it == columns_.end()) {
    throw std::runtime_error("Unknown variable: " + first_token.value);
  }

  ErasedColumn result = it->second;

  // Check for binary operator
  Token op_token = tokenizer.next();
  if (op_token.type == Token::Type::EndOfInput) {
    return result;
  }

  if (op_token.type != Token::Type::Operator) {
    throw std::runtime_error("Expected operator");
  }

  Token second_token = tokenizer.next();
  if (second_token.type != Token::Type::Variable) {
    throw std::runtime_error("Expected variable name after operator");
  }

  auto it2 = columns_.find(second_token.value);
  if (it2 == columns_.end()) {
    throw std::runtime_error("Unknown variable: " + second_token.value);
  }

  const ErasedColumn& operand2 = it2->second;

  // Perform operation
  if (op_token.value == "+") {
    return add(result, operand2);
  } else if (op_token.value == "-") {
    return subtract(result, operand2);
  } else if (op_token.value == "*") {
    return multiply(result, operand2);
  } else if (op_token.value == "&") {
    return bitwise_and(result, operand2);
  } else if (op_token.value == "|") {
    return bitwise_or(result, operand2);
  } else if (op_token.value == "^") {
    return bitwise_xor(result, operand2);
  } else {
    throw std::runtime_error("Unsupported operator: " + op_token.value);
  }
}

// Helper to delete type-erased column
inline void interpreter::delete_erased_column(ErasedColumn erased) {
  DataTypeEnum::Enum policy = erased.get_policy();
  void* ptr = erased.get_ptr();

  switch (policy) {
  case DataTypeEnum::Int32Default:
    delete std::bit_cast<column_vector<Int32DefaultPolicy>*>(ptr);
    break;
  case DataTypeEnum::Float32Default:
    delete std::bit_cast<column_vector<Float32DefaultPolicy>*>(ptr);
    break;
  case DataTypeEnum::BF16Default:
    delete std::bit_cast<column_vector<BF16DefaultPolicy>*>(ptr);
    break;
  default:
    throw std::runtime_error("Unknown policy type in delete_erased_column");
  }
}

// Arithmetic operations
inline ErasedColumn interpreter::add(const ErasedColumn& a,
                                     const ErasedColumn& b) {
  // Check that types match
  if (a.get_policy() != b.get_policy()) {
    throw std::runtime_error("Type mismatch in addition");
  }

  DataTypeEnum::Enum policy = a.get_policy();

  switch (policy) {
  case DataTypeEnum::Int32Default: {
    auto* a_ptr =
        std::bit_cast<column_vector<Int32DefaultPolicy>*>(a.get_ptr());
    auto* b_ptr =
        std::bit_cast<column_vector<Int32DefaultPolicy>*>(b.get_ptr());
    auto* result = new column_vector<Int32DefaultPolicy>(*a_ptr + *b_ptr);
    return ErasedColumn(result);
  }
  case DataTypeEnum::Float32Default: {
    auto* a_ptr =
        std::bit_cast<column_vector<Float32DefaultPolicy>*>(a.get_ptr());
    auto* b_ptr =
        std::bit_cast<column_vector<Float32DefaultPolicy>*>(b.get_ptr());
    // TODO: Implement float addition
    throw std::runtime_error("Float32 addition not yet implemented");
  }
  case DataTypeEnum::BF16Default: {
    auto* a_ptr = std::bit_cast<column_vector<BF16DefaultPolicy>*>(a.get_ptr());
    auto* b_ptr = std::bit_cast<column_vector<BF16DefaultPolicy>*>(b.get_ptr());
    auto* result = new column_vector<BF16DefaultPolicy>(*a_ptr + *b_ptr);
    return ErasedColumn(result);
  }
  default:
    throw std::runtime_error("Unknown policy type in add");
  }
}

inline ErasedColumn interpreter::subtract(const ErasedColumn& a,
                                          const ErasedColumn& b) {
  throw std::runtime_error("Subtraction not yet implemented");
}

inline ErasedColumn interpreter::multiply(const ErasedColumn& a,
                                          const ErasedColumn& b) {
  throw std::runtime_error("Multiplication not yet implemented");
}

inline ErasedColumn interpreter::bitwise_and(const ErasedColumn& a,
                                             const ErasedColumn& b) {
  throw std::runtime_error("Bitwise AND not yet implemented");
}

inline ErasedColumn interpreter::bitwise_or(const ErasedColumn& a,
                                            const ErasedColumn& b) {
  throw std::runtime_error("Bitwise OR not yet implemented");
}

inline ErasedColumn interpreter::bitwise_xor(const ErasedColumn& a,
                                             const ErasedColumn& b) {
  throw std::runtime_error("Bitwise XOR not yet implemented");
}

// Public method implementations
inline ErasedColumn interpreter::eval(const std::string& expression) {
  Tokenizer tokenizer(expression);
  return parse_expression(tokenizer);
}

inline ErasedColumn interpreter::get_column(const std::string& name) const {
  auto it = columns_.find(name);
  if (it == columns_.end()) {
    throw std::runtime_error("Unknown variable: " + name);
  }
  return it->second;
}

inline bool interpreter::has_column(const std::string& name) const {
  return columns_.find(name) != columns_.end();
}

inline void interpreter::unregister_column(const std::string& name) {
  auto it = columns_.find(name);
  if (it != columns_.end()) {
    delete_erased_column(it->second);
    columns_.erase(it);
  }
}

inline void interpreter::clear() {
  for (auto& [name, erased] : columns_) {
    delete_erased_column(erased);
  }
  columns_.clear();
}

inline size_t interpreter::size() const {
  return columns_.size();
}

} // namespace franklin

#endif // FRANKLIN_CORE_INTERPRETER_HPP
