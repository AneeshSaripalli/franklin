#include "core/expression/parser.hpp"
#include "core/compiler_macros.hpp"
#include <algorithm>
#include <cctype>
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
  enum Enum { UNDEF = 0, LITERAL = 1, COL_REF = 2, BINARY_OP = 3 };
};

constexpr static std::pair<char, std::uint8_t> operators[]{
    std::make_pair('-', 5),  std::make_pair('+', 10), std::make_pair('/', 15),
    std::make_pair('*', 20), std::make_pair('^', 25), std::make_pair('$', 0)};

static constexpr auto op_binding_power(char op) {
  auto itr = std::find_if(std::begin(operators), std::end(operators),
                          [op](auto const& opg) { return opg.first == op; });
  FRANKLIN_ASSERT(itr != std::end(operators));
  return itr->second;
}

static std::string::size_type
find_corresponding_close(std::string_view data, std::size_t opening_index) {
  FRANKLIN_ASSERT(data[opening_index] == SCOPE_OPEN);

  std::int32_t depth = 1;
  auto i = opening_index + 1;
  while (i < data.size()) {
    depth +=
        ((data[i] == SCOPE_OPEN) ? 1 : 0) + ((data[i] == SCOPE_CLOSE) ? -1 : 0);

    if (depth < 0) [[unlikely]] {
      return std::string::npos;
    } else if (!depth) {
      return i;
    }
  }
  return std::string::npos;
}

ParseResult parse(std::string_view data) {
  errors::Errors errors{};
  if (!validate_ok(data, errors)) [[unlikely]] {
    // report errors here../.
    return errors;
  }

  auto next_char = [data](std::size_t pos) {
    while (pos < data.size() && std::isspace(data[pos])) {
      ++pos;
    }
    return pos;
  };

  // Group parsing - whenever we identify an operator, we can pop and group
  // everything with binding precedence greater than the current operator.

  std::size_t index{};
  index = next_char(index);

  std::vector<std::string> sexpr_st{};
  std::vector<std::pair<char, ::ssize_t>> op_st{};

  [[maybe_unused]] auto print_stacks = [&]() {
    std::cout << fmt::format("Sexpr stack:\t");
    if (sexpr_st.size()) {
      for (auto sexpr : sexpr_st) {
        std::cout << fmt::format("({}), ", sexpr);
      }
      std::cout << std::endl;
    }

    std::cout << fmt::format("op stack:\t");
    if (op_st.size()) {
      for (auto const& op : op_st) {
        std::cout << fmt::format("(op={}, index={}), ", op.first, op.second);
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  };

  constexpr std::string_view SCOPE_OPEN_MARKER = "SCOPE_OPEN";

  auto apply_last_op = [&]() {
    const auto last_op = op_st.back();
    op_st.pop_back();
    // While pre-empting, pop two off the operator stack, then push the
    // combined result back onto the stack.
    FRANKLIN_ASSERT(sexpr_st.size() >= 2);
    auto first_sexpr = sexpr_st.back();
    sexpr_st.pop_back();
    auto second_sexpr = sexpr_st.back();
    sexpr_st.pop_back();
    sexpr_st.push_back(
        fmt::format("({}) {} ({})", second_sexpr, last_op.first, first_sexpr));
  };

  auto process_char = [&](std::size_t index, const char ch) {
    // May need to recursively subparse groups, defined by operator precedence
    // and parentheses.
    const auto is_operator =
        std::find_if(std::begin(operators), std::end(operators),
                     [ch](auto const& op) { return op.first == ch; }) !=
        std::end(operators);

    if (is_operator) {
      auto const can_pop_opstack = [&op_st](char op) noexcept {
        if (!op_st.size()) {
          return false;
        }
        const auto top = op_st.back();
        return (top.first == SCOPE_OPEN && op == SCOPE_CLOSE) ||
               (top.first != SCOPE_OPEN &&
                op_binding_power(top.first) >= op_binding_power(op));
      };

      // Pop all operators will greater precendence, and that group
      // becomes an expression node.
      while (can_pop_opstack(ch)) {
        apply_last_op();
      }

      // OK, now need to push this new operator into the stack.
      op_st.emplace_back(ch, index);
    } else if (ch == SCOPE_OPEN) {
      op_st.emplace_back(SCOPE_OPEN, index);
      sexpr_st.emplace_back(SCOPE_OPEN_MARKER);
    } else if (ch == SCOPE_CLOSE) {
      // Note above that we collapse all decreasing prec binding ops, so our op
      // stack within our frame is in non-decreasing operation binding order.
      while (op_st.back().first != SCOPE_OPEN) {
        apply_last_op();
      }
      // op stack is a dummy op
      op_st.pop_back();
      // sexpr has an SCOPE_OPEN, then our frame-local collapsed value.
      std::swap(sexpr_st[sexpr_st.size() - 2], sexpr_st.back());
      sexpr_st.pop_back();
    } else {
      sexpr_st.emplace_back(1, ch);
    }
  };

  while (index < data.size()) {
    process_char(index, data[index]);
    ++index;
  }
  process_char(index + 1, '$');
  FRANKLIN_ASSERT(sexpr_st.size() == 1);
  FRANKLIN_ASSERT(op_st.size() == 1);
  FRANKLIN_ASSERT(
      (op_st.back() == std::make_pair<char, ::ssize_t>('$', data.size() + 1)));

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