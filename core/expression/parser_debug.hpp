#ifndef FRANKLIN_CORE_EXPRESSION_PARSER_DEBUG_HPP
#define FRANKLIN_CORE_EXPRESSION_PARSER_DEBUG_HPP

#include <cstdint>
#include <ostream>

namespace franklin::parser {

class ExprNode;
class ColRef;

std::ostream& operator<<(std::ostream& os, ExprNode const& node);
std::ostream& operator<<(std::ostream& os, ColRef const& node);

} // namespace franklin::parser

#endif // FRANKLIN_CORE_EXPRESSION_PARSER_DEBUG_HPP