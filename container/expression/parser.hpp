#pragma once

#include "ast.hpp"
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace franklin::expr {

// Token types for lexing
enum class TokenType {
  Number,     // 123, 3.14
  Identifier, // variable name
  Plus,
  Minus,
  Star,
  Slash,
  Percent, // arithmetic
  LParen,
  RParen, // ()
  And,
  Or,
  Xor,
  Not,
  Shl,
  Shr, // bitwise
  LogAnd,
  LogOr,
  LogNot, // logical
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge, // comparison
  Question,
  Colon, // ternary
  Eof,
  Invalid
};

struct Token {
  TokenType type;
  std::string value;
  size_t pos;
};

class Tokenizer {
public:
  explicit Tokenizer(std::string_view input) : input_(input), pos_(0) {}

  Token next() {
    skip_whitespace();

    if (pos_ >= input_.size()) {
      return {TokenType::Eof, "", pos_};
    }

    char ch = input_[pos_];

    // Numbers
    if (std::isdigit(ch) || (ch == '.' && pos_ + 1 < input_.size() &&
                             std::isdigit(input_[pos_ + 1]))) {
      return parse_number();
    }

    // Identifiers
    if (std::isalpha(ch) || ch == '_') {
      return parse_identifier();
    }

    // Two-character operators
    if (pos_ + 1 < input_.size()) {
      std::string two_char{ch, input_[pos_ + 1]};
      if (two_char == "&&") {
        pos_ += 2;
        return {TokenType::LogAnd, "&&", pos_ - 2};
      }
      if (two_char == "||") {
        pos_ += 2;
        return {TokenType::LogOr, "||", pos_ - 2};
      }
      if (two_char == "==") {
        pos_ += 2;
        return {TokenType::Eq, "==", pos_ - 2};
      }
      if (two_char == "!=") {
        pos_ += 2;
        return {TokenType::Ne, "!=", pos_ - 2};
      }
      if (two_char == "<=") {
        pos_ += 2;
        return {TokenType::Le, "<=", pos_ - 2};
      }
      if (two_char == ">=") {
        pos_ += 2;
        return {TokenType::Ge, ">=", pos_ - 2};
      }
      if (two_char == "<<") {
        pos_ += 2;
        return {TokenType::Shl, "<<", pos_ - 2};
      }
      if (two_char == ">>") {
        pos_ += 2;
        return {TokenType::Shr, ">>", pos_ - 2};
      }
    }

    // Single-character operators
    size_t start = pos_++;
    switch (ch) {
    case '+':
      return {TokenType::Plus, "+", start};
    case '-':
      return {TokenType::Minus, "-", start};
    case '*':
      return {TokenType::Star, "*", start};
    case '/':
      return {TokenType::Slash, "/", start};
    case '%':
      return {TokenType::Percent, "%", start};
    case '(':
      return {TokenType::LParen, "(", start};
    case ')':
      return {TokenType::RParen, ")", start};
    case '&':
      return {TokenType::And, "&", start};
    case '|':
      return {TokenType::Or, "|", start};
    case '^':
      return {TokenType::Xor, "^", start};
    case '~':
      return {TokenType::Not, "~", start};
    case '!':
      return {TokenType::LogNot, "!", start};
    case '<':
      return {TokenType::Lt, "<", start};
    case '>':
      return {TokenType::Gt, ">", start};
    case '?':
      return {TokenType::Question, "?", start};
    case ':':
      return {TokenType::Colon, ":", start};
    default:
      return {TokenType::Invalid, std::string(1, ch), start};
    }
  }

  Token peek() {
    size_t saved_pos = pos_;
    Token tok = next();
    pos_ = saved_pos;
    return tok;
  }

private:
  std::string_view input_;
  size_t pos_;

  void skip_whitespace() {
    while (pos_ < input_.size() && std::isspace(input_[pos_])) {
      pos_++;
    }
  }

  Token parse_number() {
    size_t start = pos_;
    bool has_dot = false;

    while (pos_ < input_.size()) {
      char ch = input_[pos_];
      if (std::isdigit(ch)) {
        pos_++;
      } else if (ch == '.' && !has_dot) {
        has_dot = true;
        pos_++;
      } else {
        break;
      }
    }

    return {TokenType::Number, std::string(input_.substr(start, pos_ - start)),
            start};
  }

  Token parse_identifier() {
    size_t start = pos_;

    while (pos_ < input_.size()) {
      char ch = input_[pos_];
      if (std::isalnum(ch) || ch == '_') {
        pos_++;
      } else {
        break;
      }
    }

    return {TokenType::Identifier,
            std::string(input_.substr(start, pos_ - start)), start};
  }
};

class ExpressionParser {
public:
  // Parse with column context (for type lookup)
  std::unique_ptr<ExprNode>
  parse(std::string_view expr_str,
        const std::unordered_map<std::string, DataType>& column_types) {
    tokenizer_ = std::make_unique<Tokenizer>(expr_str);
    column_types_ = &column_types;
    current_ = tokenizer_->next();
    return parse_expression(0);
  }

private:
  std::unique_ptr<Tokenizer> tokenizer_;
  const std::unordered_map<std::string, DataType>* column_types_ = nullptr;
  Token current_;

  void advance() { current_ = tokenizer_->next(); }

  bool expect(TokenType type) {
    if (current_.type != type) {
      return false;
    }
    advance();
    return true;
  }

  int precedence(TokenType type) {
    static const std::unordered_map<TokenType, int> prec = {
        {TokenType::LogOr, 1}, {TokenType::LogAnd, 2}, {TokenType::Or, 3},
        {TokenType::Xor, 4},   {TokenType::And, 5},    {TokenType::Eq, 6},
        {TokenType::Ne, 6},    {TokenType::Lt, 7},     {TokenType::Le, 7},
        {TokenType::Gt, 7},    {TokenType::Ge, 7},     {TokenType::Shl, 8},
        {TokenType::Shr, 8},   {TokenType::Plus, 9},   {TokenType::Minus, 9},
        {TokenType::Star, 10}, {TokenType::Slash, 10}, {TokenType::Percent, 10},
    };

    auto it = prec.find(type);
    return it != prec.end() ? it->second : -1;
  }

  OpType token_to_op(TokenType type) {
    switch (type) {
    case TokenType::Plus:
      return OpType::Add;
    case TokenType::Minus:
      return OpType::Sub;
    case TokenType::Star:
      return OpType::Mul;
    case TokenType::Slash:
      return OpType::Div;
    case TokenType::Percent:
      return OpType::Mod;
    case TokenType::And:
      return OpType::BitAnd;
    case TokenType::Or:
      return OpType::BitOr;
    case TokenType::Xor:
      return OpType::BitXor;
    case TokenType::Shl:
      return OpType::Shl;
    case TokenType::Shr:
      return OpType::Shr;
    case TokenType::Eq:
      return OpType::Eq;
    case TokenType::Ne:
      return OpType::Ne;
    case TokenType::Lt:
      return OpType::Lt;
    case TokenType::Le:
      return OpType::Le;
    case TokenType::Gt:
      return OpType::Gt;
    case TokenType::Ge:
      return OpType::Ge;
    case TokenType::LogAnd:
      return OpType::LogicalAnd;
    case TokenType::LogOr:
      return OpType::LogicalOr;
    default:
      throw std::runtime_error("Invalid operator token");
    }
  }

  // Precedence climbing algorithm
  std::unique_ptr<ExprNode> parse_expression(int min_prec) {
    auto left = parse_primary();

    while (true) {
      int prec = precedence(current_.type);
      if (prec < min_prec) {
        break;
      }

      TokenType op_token = current_.type;
      advance();

      // Right-associative for same precedence (not needed for our ops, but good
      // practice)
      auto right = parse_expression(prec + 1);

      left = std::make_unique<BinaryOpNode>(token_to_op(op_token),
                                            std::move(left), std::move(right));
    }

    // Handle ternary operator
    if (current_.type == TokenType::Question) {
      advance();
      auto true_branch = parse_expression(0);

      if (!expect(TokenType::Colon)) {
        throw std::runtime_error("Expected ':' in ternary operator");
      }

      auto false_branch = parse_expression(0);

      left = std::make_unique<TernaryNode>(
          std::move(left), std::move(true_branch), std::move(false_branch));
    }

    return left;
  }

  std::unique_ptr<ExprNode> parse_primary() {
    // Unary operators
    if (current_.type == TokenType::Minus || current_.type == TokenType::Not ||
        current_.type == TokenType::LogNot) {
      TokenType op_token = current_.type;
      advance();
      auto child = parse_primary();

      OpType op;
      if (op_token == TokenType::Minus) {
        // Unary minus: -x is 0 - x
        auto zero = std::make_unique<ConstantNode>(0.0);
        return std::make_unique<BinaryOpNode>(OpType::Sub, std::move(zero),
                                              std::move(child));
      } else if (op_token == TokenType::Not) {
        op = OpType::BitNot;
      } else {
        op = OpType::LogicalNot;
      }

      return std::make_unique<UnaryOpNode>(op, std::move(child));
    }

    // Parentheses
    if (current_.type == TokenType::LParen) {
      advance();
      auto expr = parse_expression(0);
      if (!expect(TokenType::RParen)) {
        throw std::runtime_error("Expected closing ')'");
      }
      return expr;
    }

    // Number literal
    if (current_.type == TokenType::Number) {
      std::string value = current_.value;
      advance();

      if (value.find('.') != std::string::npos) {
        return std::make_unique<ConstantNode>(std::stod(value));
      } else {
        return std::make_unique<ConstantNode>(
            static_cast<int64_t>(std::stoll(value)));
      }
    }

    // Identifier (column reference or type cast)
    if (current_.type == TokenType::Identifier) {
      std::string name = current_.value;
      advance();

      // Check if this is a type cast: int32(expr)
      if (current_.type == TokenType::LParen) {
        DataType cast_type = string_to_type(name);
        if (cast_type != DataType::Unknown) {
          advance(); // consume '('
          auto child = parse_expression(0);
          if (!expect(TokenType::RParen)) {
            throw std::runtime_error("Expected ')' after cast");
          }
          return std::make_unique<CastNode>(cast_type, std::move(child));
        }
      }

      // Column reference
      auto node = std::make_unique<ColumnRefNode>(name);

      // Look up type from context
      if (column_types_) {
        auto it = column_types_->find(name);
        if (it != column_types_->end()) {
          node->result_type = it->second;
        } else {
          throw std::runtime_error("Unknown column: " + name);
        }
      }

      return node;
    }

    throw std::runtime_error("Unexpected token: " + current_.value);
  }

  DataType string_to_type(const std::string& str) {
    static const std::unordered_map<std::string, DataType> types = {
        {"int8", DataType::Int8},       {"int16", DataType::Int16},
        {"int32", DataType::Int32},     {"int64", DataType::Int64},
        {"uint8", DataType::UInt8},     {"uint16", DataType::UInt16},
        {"uint32", DataType::UInt32},   {"uint64", DataType::UInt64},
        {"float16", DataType::Float16}, {"float32", DataType::Float32},
        {"float64", DataType::Float64}, {"double", DataType::Float64},
        {"bf16", DataType::BF16},       {"bool", DataType::Bool},
    };

    auto it = types.find(str);
    return it != types.end() ? it->second : DataType::Unknown;
  }
};

} // namespace franklin::expr
