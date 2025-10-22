
#ifndef FRANKLIN_CORE_EXPRESSION_PARSER_HPP
#define FRANKLIN_CORE_EXPRESSION_PARSER_HPP

#include <list>
#include <memory>
#include <string_view>
#include <variant>

namespace franklin {
namespace parser {

class ParsedDataTypeEnum {
public:
  enum Enum : std::uint16_t {
    None = 0,
    Unknown,
    Int32,
    Float32,
    BF16,
  };
};

class ASTNode {};

class ExprNode : public ASTNode {};

class BinaryOpNode : public ExprNode {};

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

using ParseResult =
    std::variant<std::monostate, std::unique_ptr<ASTNode>, errors::Errors>;

ParseResult parse(std::string_view data);

bool parse_result_ok(ParseResult const& parse_result) noexcept;
errors::Errors
parse_result_extract_errors(ParseResult const& parse_result) noexcept;

} // namespace parser
} // namespace franklin

#endif // FRANKLIN_CORE_EXPRESSION_PARSER_HPP