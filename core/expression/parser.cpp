#include "core/expression/parser.hpp"
#include <algorithm>
#include <fmt/format.h>
#include <list>
#include <variant>
#include <vector>

namespace franklin::parser {

bool errors::Errors::has_error() const noexcept {
  return error_list.size();
}

static constexpr char SCOPE_OPEN = '(';
static constexpr char SCOPE_CLOSE = ')';

static void check_parens(std::string_view data, errors::Errors& errors) {
  std::vector<std::int32_t> opening_index{};
  opening_index.reserve(std::count(data.begin(), data.end(), SCOPE_OPEN));

  for (std::size_t pos = 0; pos < data.size(); ++pos) {
    auto const c = data[pos];
    if (c == SCOPE_OPEN) {
      opening_index.push_back(pos);
    } else if (c == SCOPE_CLOSE) {
      if (opening_index.empty()) [[unlikely]] {
        errors.error_list.emplace_back(
            pos, fmt::format(
                     "[[UnopenedScopeError]]: Parsing error: closed "
                     "scope token ')' at parsing index {} for unopened scope.",
                     pos));
      } else {
        opening_index.pop_back();
      }
    }
  }

  while (opening_index.size()) {
    errors.error_list.emplace_back(
        opening_index.back(),
        fmt::format("[[UnclosedScopeError]]: Parsing error: opened scope token "
                    "'(' at parsing index {} for closed scope.",
                    opening_index.back()));
    opening_index.pop_back();
  }
}

static bool validate_ok(std::string_view data, errors::Errors& errors) {
  check_parens(data, errors);
  return !errors.has_error();
}

struct ParserTokenState {
  enum Enum {
    UNDEF = 0,
    LITERAL = 1,
    COL_REF = 2,
    BINARY_OP = 3,
    ASSIGNMENT = 4
  };
};

ParseResult parse(std::string_view data) {
  errors::Errors errors{};
  if (!validate_ok(data, errors)) [[unlikely]] {
    // report errors here../.
    return errors;
  }

  // Stupid ass parsing idea - I think I can improve on this in the future.
  // Find all clauses we know to be at depth 0 (root depth), then continue with
  // greater subdepths using recursive subparsing.
  // We stop recursing when we run across something of type COL_REF
  for (auto i = 0; i < data.size(); ++i) {
  }

  return nullptr;
}

bool parse_result_ok(ParseResult const& parse_result) noexcept {
  return std::holds_alternative<std::unique_ptr<ASTNode>>(parse_result);
}

errors::Errors
parse_result_extract_errors(ParseResult const& parse_result) noexcept {
  return std::holds_alternative<errors::Errors>(parse_result)
             ? std::get<errors::Errors>(parse_result)
             : errors::Errors{};
}

std::variant<std::monostate, LiteralNode, errors::Errors>
LiteralNode::parse_from_data(std::string_view data) noexcept {
  errors::Errors errors{};

  auto const underscores = std::count(data.begin(), data.end(), '_');
  if (underscores != 1) [[unlikely]] {
    if (underscores > 1) {
      const auto second_underscore_index = std::distance(
          data.begin(), std::find(std::find(data.begin(), data.end(), '_') + 1,
                                  data.end(), '_'));
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
        fmt::format("Expected valid type marker value, found {}", type_marker));
    return errors;
  }
  return LiteralNode{literal_data, type_marker_enum};
}

} // namespace franklin::parser