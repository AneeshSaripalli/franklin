
#ifndef FRANKLIN_CORE_EXPRESSION_PARSER_HPP
#define FRANKLIN_CORE_EXPRESSION_PARSER_HPP

#include "core/bf16.hpp"
#include "core/compiler_macros.hpp"
#include "core/data_type_enum.hpp"
#include <charconv>
#include <fmt/format.h>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

namespace franklin {
namespace parser {

namespace errors {
struct ParseError {
  std::size_t pos;
  std::string desc;
};

struct Errors {
  std::list<ParseError> error_list;
  bool has_error() const noexcept;
};

} // namespace errors

// TODO: Add more as we support them in column.hpp.
// E.g. bitwise shl, shr, xor, and, add, etc.
struct BinaryOp {
  enum Enum : std::uint16_t {
    NONE = 0,
    ADD = 1,
    SUB = 2,
    MUL = 3,
    UNKNOWN = std::numeric_limits<std::underlying_type_t<Enum>>::max()
  };

  static constexpr BinaryOp::Enum from_string(char ch) noexcept {
    switch (ch) {
    case '+':
      return Enum::ADD;
    case '-':
      return Enum::SUB;
    case '*':
      return Enum::MUL;
    default:
      return Enum::NONE;
    }
  }

  constexpr static std::string_view to_string(Enum e) noexcept {
    switch (e) {
    case Enum::NONE:
      return "NONE";
    case Enum::ADD:
      return "ADD";
    case Enum::SUB:
      return "SUB";
    case Enum::MUL:
      return "MUL";
    default:
      return "UNKNOWN";
    }
  }
};

struct ExprNodeType {
  enum Enum : std::uint16_t {
    NONE = 0,
    LITERAL = 1,
    COL_REF = 2,
    BINARY_OP = 3
  };

  static constexpr std::string_view to_string(Enum e) noexcept {
    switch (e) {
    case Enum::NONE:
      return "NONE";
    case Enum::LITERAL:
      return "LITERAL";
    case Enum::COL_REF:
      return "COL_REF";
    case Enum::BINARY_OP:
      return "BINARY_OP";
    default:
      return "UNKNOWN";
    }
  }
};

class ExprNode {
protected:
  DataTypeEnum::Enum result_;

public:
  ExprNode() : result_(DataTypeEnum::Unknown) {}
  ExprNode(DataTypeEnum::Enum result) : result_(result) {}
  virtual ~ExprNode() = default;

  virtual DataTypeEnum::Enum result() const noexcept { return result_; }

  virtual std::string to_string() const noexcept = 0;
  virtual std::string enriched_representation() const noexcept = 0;
  virtual ExprNodeType::Enum node_type() const noexcept = 0;

  bool operator==(ExprNode const& other) const;
};

class ColRef final : public ExprNode {
  std::string col_name_;

public:
  ColRef(std::string col_name, DataTypeEnum::Enum result)
      : ExprNode{result}, col_name_{col_name} {}
  auto name() const noexcept { return col_name_; }

  virtual std::string to_string() const noexcept override {
    return fmt::format("({})", col_name_);
  }

  virtual bool operator==(ColRef const& ref) const noexcept {
    return col_name_ == ref.col_name_;
  }

  virtual ExprNodeType::Enum node_type() const noexcept override {
    return ExprNodeType::COL_REF;
  }

  virtual std::string enriched_representation() const noexcept override {
    return fmt::format("ColRef(name={})", col_name_);
  }
};

// Literals are ambigious. Literal expressions require both a value and a type
// marker. Examples:
// -- int32 literal: 2018_i32
// -- float32 literal: 2.018_f32
// -- bf16 literal: 2.0_bf16
class LiteralNode : public ExprNode {
  std::string_view literal_{};
  DataTypeEnum::Enum type_;

  constexpr static DataTypeEnum::Enum
  parse_type_marker(std::string_view type_marker) {
    using namespace std::string_view_literals;

    if (type_marker == "i32"sv) {
      return DataTypeEnum::Int32Default;
    } else if (type_marker == "f32"sv) {
      return DataTypeEnum::Float32Default;
    } else if (type_marker == "bf16"sv) {
      return DataTypeEnum::BF16Default;
    } else {
      return DataTypeEnum::Unknown;
    }
  }

  LiteralNode() noexcept = default;

public:
  static std::variant<std::monostate, LiteralNode, errors::Errors>
  parse_from_data(std::string_view data) noexcept;

  LiteralNode(std::string_view literal_sv, DataTypeEnum::Enum type) noexcept
      : literal_(literal_sv), type_(type) {}
  virtual ~LiteralNode() = default;

  template <typename VisitorT> void visit(VisitorT&& visitor) const {
    switch (type_) {
    case DataTypeEnum::Int32Default: {
      std::int32_t value{};
      auto [ptr, ec] = std::from_chars(
          literal_.data(), literal_.data() + literal_.size(), value);
      bool const good_full_parse =
          (ptr == literal_.data() + literal_.size()) && (ec == std::errc{});
      FRANKLIN_ASSERT(good_full_parse);
      visitor(value);
      break;
    }
    case DataTypeEnum::Float32Default: {
      float value{};
      auto [ptr, ec] = std::from_chars(
          literal_.data(), literal_.data() + literal_.size(), value);
      bool const good_full_parse =
          (ptr == literal_.data() + literal_.size()) && (ec == std::errc{});
      FRANKLIN_ASSERT(good_full_parse);
      visitor(value);
      break;
    }
    case DataTypeEnum::BF16Default: {
      float value{};
      auto [ptr, ec] = std::from_chars(
          literal_.data(), literal_.data() + literal_.size(), value);
      bool const good_full_parse =
          (ptr == literal_.data() + literal_.size()) && (ec == std::errc{});
      FRANKLIN_ASSERT(good_full_parse);

      // Technically, we should probably warn if the lower bits are getting
      // truncated. Should we warn if the literal can't be represented by the
      // type marker's domain?

      const auto [value_bf16, exactly_represented] = bf16::from_float(value);
      if (!exactly_represented) {
        auto const err = fmt::format("Representing the literal {} as a bf16 "
                                     "results in a loss of precision.",
                                     literal_);
        FRANKLIN_ABORT(err.c_str());
      }

      visitor(bf16::from_float_trunc(value));
      break;
    }
    default:
      break;
    }
  }

  virtual DataTypeEnum::Enum result() const noexcept override { return type_; }

  virtual std::string to_string() const noexcept override {
    return fmt::format("({} : {})", literal_.substr(0, literal_.find('_')),
                       DataTypeEnum::to_string(type_));
  }

  virtual ExprNodeType::Enum node_type() const noexcept override {
    return ExprNodeType::LITERAL;
  }

  virtual bool operator==(LiteralNode const& other) const noexcept {
    return std::tie(literal_, type_) == std::tie(other.literal_, other.type_);
  }

  virtual std::string enriched_representation() const noexcept override {
    return fmt::format("LiteralNode(literal={},type={})", literal_,
                       DataTypeEnum::to_string(type_));
  }
};

class BinaryOpNode : public ExprNode {
public:
  BinaryOpNode(BinaryOp::Enum op, std::unique_ptr<ExprNode> left,
               std::unique_ptr<ExprNode> right)
      : op_(op), left_(std::move(left)), right_(std::move(right)) {}

private:
  BinaryOp::Enum op_;
  std::unique_ptr<ExprNode> left_;
  std::unique_ptr<ExprNode> right_;

public:
  auto op() const noexcept { return op_; }
  auto left() const noexcept { return left_.get(); }
  auto right() const noexcept { return right_.get(); }

  virtual bool operator==(BinaryOpNode const& other) const noexcept {
    return op_ == other.op_ && *left_ == *other.left_ &&
           *right_ == *other.right_;
  }

  virtual std::string to_string() const noexcept {
    char op_char;
    switch (op_) {
    case BinaryOp::ADD:
      op_char = '+';
      break;
    case BinaryOp::SUB:
      op_char = '-';
      break;
    case BinaryOp::MUL:
      op_char = '*';
      break;
    default:
      op_char = '?';
      break;
    }
    return fmt::format("({}{}{})", left_->to_string(), op_char,
                       right_->to_string());
  }

  virtual ExprNodeType::Enum node_type() const noexcept override {
    return ExprNodeType::BINARY_OP;
  }

  virtual std::string enriched_representation() const noexcept {
    return fmt::format(
        "BinaryOpNode(op={},left={},right={})", BinaryOp::to_string(op_),
        left_->enriched_representation(), right_->enriched_representation());
  }
};

using ParseResult =
    std::variant<std::monostate, std::unique_ptr<ExprNode>, errors::Errors>;

ParseResult parse(std::string_view data);

bool parse_result_ok(ParseResult const& parse_result) noexcept;
std::unique_ptr<ExprNode> extract_result(ParseResult&& parse_result);
errors::Errors
parse_result_extract_errors(ParseResult const& parse_result) noexcept;

} // namespace parser
} // namespace franklin

#endif // FRANKLIN_CORE_EXPRESSION_PARSER_HPP