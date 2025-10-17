#ifndef FRANKLIN_CONTAINER_INTERPRETER_HPP
#define FRANKLIN_CONTAINER_INTERPRETER_HPP

#include "container/column.hpp"
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace franklin {

template <concepts::ColumnPolicy Policy> class interpreter {
public:
  using column_type = column_vector<Policy>;
  using value_type = typename Policy::value_type;

private:
  std::unordered_map<std::string, column_type> columns_;

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

    void skip_whitespace() {
      while (pos_ < input_.size() && std::isspace(input_[pos_])) {
        ++pos_;
      }
    }

  public:
    explicit Tokenizer(std::string_view input) : input_(input) {}

    Token next() {
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
  };

  // Simple recursive descent parser for binary expressions
  column_type parse_expression(Tokenizer& tokenizer) {
    Token first_token = tokenizer.next();

    if (first_token.type != Token::Type::Variable) {
      throw std::runtime_error("Expected variable name");
    }

    auto it = columns_.find(first_token.value);
    if (it == columns_.end()) {
      throw std::runtime_error("Unknown variable: " + first_token.value);
    }

    column_type result = it->second;

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

    const column_type& operand2 = it2->second;

    // Perform operation (placeholder - actual operations need to be
    // implemented)
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

  // Placeholder arithmetic operations - these need proper implementation
  static column_type add(const column_type& a, const column_type& b) {
    // TODO: Implement element-wise addition
    throw std::runtime_error("Addition not yet implemented");
  }

  static column_type subtract(const column_type& a, const column_type& b) {
    // TODO: Implement element-wise subtraction
    throw std::runtime_error("Subtraction not yet implemented");
  }

  static column_type multiply(const column_type& a, const column_type& b) {
    // TODO: Implement element-wise multiplication
    throw std::runtime_error("Multiplication not yet implemented");
  }

  static column_type bitwise_and(const column_type& a, const column_type& b) {
    // TODO: Implement element-wise bitwise AND
    throw std::runtime_error("Bitwise AND not yet implemented");
  }

  static column_type bitwise_or(const column_type& a, const column_type& b) {
    // TODO: Implement element-wise bitwise OR
    throw std::runtime_error("Bitwise OR not yet implemented");
  }

  static column_type bitwise_xor(const column_type& a, const column_type& b) {
    // TODO: Implement element-wise bitwise XOR
    throw std::runtime_error("Bitwise XOR not yet implemented");
  }

public:
  interpreter() = default;

  // Register a column vector with a name
  void register_column(const std::string& name, const column_type& col) {
    columns_[name] = col;
  }

  void register_column(const std::string& name, column_type&& col) {
    columns_[name] = std::move(col);
  }

  // Evaluate an expression
  column_type eval(const std::string& expression) {
    Tokenizer tokenizer(expression);
    return parse_expression(tokenizer);
  }

  // Check if a name is registered
  bool has_column(const std::string& name) const {
    return columns_.find(name) != columns_.end();
  }

  // Get a column by name
  const column_type& get_column(const std::string& name) const {
    auto it = columns_.find(name);
    if (it == columns_.end()) {
      throw std::runtime_error("Unknown variable: " + name);
    }
    return it->second;
  }

  // Remove a column
  void unregister_column(const std::string& name) { columns_.erase(name); }

  // Clear all columns
  void clear() { columns_.clear(); }

  // Get number of registered columns
  size_t size() const { return columns_.size(); }
};

} // namespace franklin

#endif // FRANKLIN_CONTAINER_INTERPRETER_HPP
