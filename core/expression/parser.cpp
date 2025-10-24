#include "core/expression/parser.hpp"
#include "core/compiler_macros.hpp"
#include <algorithm>
#include <cctype>
#include <fmt/format.h>
#include <iostream>
#include <list>
#include <optional>
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

static constexpr std::pair<bool, std::uint8_t>
find_operator(const char ch) noexcept {
  auto itr = std::find_if(std::begin(operators), std::end(operators),
                          [ch](auto const& op) { return op.first == ch; });
  if (itr == std::end(operators)) {
    return {false, 0UL};
  }

  return {true, itr->second};
}

class Parser {
  std::string_view data_;
  std::vector<std::unique_ptr<ExprNode>> expr_st{};
  std::vector<std::pair<char, ::ssize_t>> op_st{};

private:
  void apply_last_op() noexcept {
    FRANKLIN_ASSERT(!op_st.empty());
    const auto [op_ch, op_index] = op_st.back();
    op_st.pop_back();

    FRANKLIN_ASSERT(expr_st.size() >= 2);
    auto first_expr = std::move(expr_st.back());
    expr_st.pop_back();
    auto second_expr = std::move(expr_st.back());
    expr_st.pop_back();
    expr_st.push_back(std::make_unique<BinaryOpNode>(
        BinaryOp::from_string(op_ch), std::move(second_expr),
        std::move(first_expr)));
  }

  bool can_pop_opstack(char op) const noexcept {
    if (!op_st.size()) {
      return false;
    }
    const auto top = op_st.back();
    return (top.first == SCOPE_OPEN && op == SCOPE_CLOSE) ||
           (top.first != SCOPE_OPEN &&
            op_binding_power(top.first) >= op_binding_power(op));
  };

  void enqueue_operator(std::size_t index, char ch) {
    const auto [is_operator, op_binding_power] = find_operator(ch);
    FRANKLIN_ASSERT_MSG(is_operator,
                        "Operands to enqueue_operator must be an operator.");

    // Pop all operators will greater precendence, and that group
    // becomes an expression node.
    while (can_pop_opstack(ch)) {
      apply_last_op();
    }

    // OK, now need to push this new operator into the stack.
    op_st.emplace_back(ch, index);
  }

  void enqueue_sclose(std::size_t index, char ch) {
    // Note above that we collapse all decreasing prec binding ops, so our op
    // stack within our frame is in non-decreasing operation binding order.
    while (op_st.size() && (op_st.back().first != SCOPE_OPEN)) {
      apply_last_op();
    }
    // op stack is a dummy op
    FRANKLIN_ASSERT(!op_st.empty());
    FRANKLIN_ASSERT(op_st.back().first == SCOPE_OPEN);
    op_st.pop_back();

    // expr has a nullptr sentinel, handle it
    FRANKLIN_ASSERT(expr_st.size() >= 1);
    if (expr_st.size() == 1) {
      // Empty parentheses case: only sentinel present
      FRANKLIN_ASSERT(expr_st.back() == nullptr);
      // Keep the nullptr (represents empty expression)
    } else {
      // Normal case: sentinel + result
      FRANKLIN_ASSERT(expr_st.size() >= 2);
      FRANKLIN_ASSERT(expr_st[expr_st.size() - 2] == nullptr);
      auto expr_st_head = std::move(*expr_st.rbegin());
      expr_st.pop_back();
      expr_st.pop_back();
      expr_st.push_back(std::move(expr_st_head));
    }
  }

  void dump_stacks() {
    // print op_stack in a single line
    std::cout << "op_st: ";
    for (const auto& op : op_st) {
      std::cout << fmt::format("('{}', {})", op.first, op.second) << ", ";
    }
    std::cout << std::endl;
    // print expr_st in a single line
    std::cout << "expr_st: ";
    for (const auto& expr : expr_st) {
      if (expr == nullptr) {
        std::cout << fmt::format("(SCOP), ");
      } else {
        std::cout << fmt::format("{}, ", expr->to_string());
      }
    }
    std::cout << std::endl;
  }

public:
  Parser(std::string_view data) noexcept : data_{data} {}
  ~Parser() = default;

  FRANKLIN_FORCE_INLINE static constexpr bool
  is_id_char(const char ch) noexcept {
    return std::isalnum(ch) || ch == '_';
  }

  ParseResult parse() {
    errors::Errors errors{};
    if (!validate_ok(data_, errors)) [[unlikely]] {
      // report errors here../.
      return errors;
    }

    std::size_t index{};
    std::optional<std::size_t> lex_start{std::nullopt};

    auto const try_flush_lexeme = [&]() {
      if (lex_start) {
        std::string_view lexeme = data_.substr(*lex_start, index - *lex_start);
        expr_st.push_back(std::make_unique<ColRef>(std::string{lexeme},
                                                   DataTypeEnum::Unknown));
        lex_start.reset();
      }
    };

    // Logic:
    // If whitespace, end last lexeme
    // If operator, reestablish non-decreasing operator priority
    // If open/close paren, open or close the frame with expression collapse
    while (index < data_.size()) {
      const char ch = data_[index];

      const auto [is_operator, op_binding_power] = find_operator(data_[index]);

      if (is_id_char(ch)) {
        if (!lex_start) {
          lex_start = index;
        } else {
          // Else continue accumulating the lexeme from the original start
          // position.
          // Empty
        }
      } else {
        try_flush_lexeme();

        if (std::isspace(ch)) {
          // Empty
        } else if (ch == SCOPE_OPEN) {
          op_st.emplace_back(SCOPE_OPEN, index);
          expr_st.emplace_back(nullptr);
        } else if (ch == SCOPE_CLOSE) {
          enqueue_sclose(index, ch);
        } else {
          if (!is_operator) [[unlikely]] {
            const auto err =
                fmt::format("err: expected an operator, found {}", ch);
            FRANKLIN_ASSERT_MSG(is_operator, err.c_str());
          }
          // Pop all operators will greater precendence, and that group
          // becomes an expression node.
          while (can_pop_opstack(ch)) {
            apply_last_op();
          }

          // OK, now need to push this new operator into the stack.
          op_st.emplace_back(ch, index);
        }
      }
      ++index;
    }

    try_flush_lexeme();
    enqueue_operator(index + 1, '$');
    FRANKLIN_ASSERT(expr_st.size() == 1);
    FRANKLIN_ASSERT(op_st.size() == 1);
    FRANKLIN_ASSERT((op_st.back() ==
                     std::make_pair<char, ::ssize_t>('$', data_.size() + 1)));

    return std::move(expr_st.front());
  }
};

ParseResult parse(std::string_view data) {
  Parser parser{data};
  return parser.parse();
}

bool parse_result_ok(ParseResult const& parse_result) noexcept {
  return std::holds_alternative<std::unique_ptr<ExprNode>>(parse_result);
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

std::unique_ptr<ExprNode> extract_result(ParseResult&& parse_result) {
  return std::get<std::unique_ptr<ExprNode>>(std::move(parse_result));
}

bool ExprNode::operator==(ExprNode const& other) const {
  if (node_type() != other.node_type()) {
    return false;
  }

  switch (node_type()) {
  case ExprNodeType::LITERAL:
    return *static_cast<LiteralNode const*>(&other) ==
           *static_cast<LiteralNode const*>(this);
  case ExprNodeType::COL_REF:
    return *static_cast<ColRef const*>(&other) ==
           *static_cast<ColRef const*>(this);
  case ExprNodeType::BINARY_OP:
    return *static_cast<BinaryOpNode const*>(&other) ==
           *static_cast<BinaryOpNode const*>(this);
  default:
    return false;
  }
}

} // namespace franklin::parser