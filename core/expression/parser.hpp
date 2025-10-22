
#ifndef FRANKLIN_CORE_EXPRESSION_PARSER_HPP
#define FRANKLIN_CORE_EXPRESSION_PARSER_HPP

#include "core/bf16.hpp"
#include "core/compiler_macros.hpp"
#include "core/data_type_enum.hpp"
#include <algorithm>
#include <charconv>
#include <fmt/format.h>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
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
};

class ASTNode {
  virtual DataTypeEnum::Enum result() noexcept = 0;
};

class ExprNode : public ASTNode {
private:
  DataTypeEnum result_;
};

class ColRef : public ExprNode {
  std::string col_name_;

public:
  auto name() const noexcept { return col_name_; }
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
  parse_from_data(std::string_view data) noexcept {
    errors::Errors errors{};

    auto const underscores = std::count(data.begin(), data.end(), '_');
    if (underscores != 1) [[unlikely]] {
      if (underscores > 1) {
        const auto second_underscore_index = std::distance(
            data.begin(),
            std::find(std::find(data.begin(), data.end(), '_') + 1, data.end(),
                      '_'));
        errors.error_list.emplace_back(
            second_underscore_index,
            fmt::format(
                "Parsing literal {} with multiple underscores. Expected just "
                "one underscore for the type marker after the literal. Found "
                "{} "
                "underscores, the second with index {}",
                data, underscores, second_underscore_index));
      } else {
        errors.error_list.emplace_back(
            0, fmt::format("Found no type marker when parsing data literal {}. "
                           "Expected one, exactly, to denote the type marker.",
                           data));
      }
      return errors; // Not attempting recovery here. That's a standard way of
                     // providing more feedback to the user. Not attempting
                     // that here/ right now.
    }

    auto const underscore_itr = std::find(data.begin(), data.end(), '_');
    std::string_view const literal_data =
        std::string_view{data.begin(), underscore_itr};
    auto start_type_marker = underscore_itr;
    std::advance(start_type_marker, 1.0);
    std::string_view const type_marker =
        std::string_view{start_type_marker, data.end()};

    auto const type_marker_enum = parse_type_marker(type_marker);
    if (type_marker_enum == DataTypeEnum::Unknown) [[unlikely]] {
      errors.error_list.emplace_back(
          std::distance(data.begin(), start_type_marker),
          fmt::format("Expected valid type marker value, found {}",
                      type_marker));
      return errors;
    }
    return LiteralNode{literal_data, type_marker_enum};
  }

  LiteralNode(std::string_view literal_sv, DataTypeEnum::Enum type) noexcept
      : literal_(literal_sv), type_(type) {}

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
    }
    case DataTypeEnum::Float32Default: {
      float value{};
      auto [ptr, ec] = std::from_chars(
          literal_.data(), literal_.data() + literal_.size(), value);
      bool const good_full_parse =
          (ptr == literal_.data() + literal_.size()) && (ec == std::errc{});
      FRANKLIN_ASSERT(good_full_parse);
      visitor(value);
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
    }
    default:
      break;
    }
  }

  virtual DataTypeEnum::Enum result() noexcept override { return type_; }
};

class BinaryOpNode : public ExprNode {
public:
  BinaryOpNode(BinaryOp op, std::unique_ptr<ExprNode> left,
               std::unique_ptr<ExprNode> right)
      : op_(op), left_(std::move(left)), right_(std::move(right)) {}

private:
  BinaryOp op_;
  std::unique_ptr<ExprNode> left_;
  std::unique_ptr<ExprNode> right_;
};

using ParseResult =
    std::variant<std::monostate, std::unique_ptr<ASTNode>, errors::Errors>;

ParseResult parse(std::string_view data);

bool parse_result_ok(ParseResult const& parse_result) noexcept;
errors::Errors
parse_result_extract_errors(ParseResult const& parse_result) noexcept;

} // namespace parser
} // namespace franklin

#endif // FRANKLIN_CORE_EXPRESSION_PARSER_HPP