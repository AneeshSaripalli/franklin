#include "core/expression/parser_debug.hpp"
#include "core/data_type_enum.hpp"
#include "parser.hpp"

#include <fmt/format.h>

namespace franklin::parser {

std::ostream& operator<<(std::ostream& os, ExprNode const& node) {
  os << fmt::format("ExprNode(output={})",
                    DataTypeEnum::to_string(node.result()));
  return os;
}

std::ostream& operator<<(std::ostream& os, ColRef const& node) {
  os << fmt::format("ColRef(output={},name={})",
                    DataTypeEnum::to_string(node.result()), node.name());
  return os;
}

} // namespace franklin::parser