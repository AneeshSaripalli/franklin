#ifndef FRANKLIN_CORE_DATA_TYPE_ENUM_HPP
#define FRANKLIN_CORE_DATA_TYPE_ENUM_HPP

#include <cstdint>
#include <limits>
#include <type_traits>

namespace franklin {

class DataTypeEnum {
public:
  enum Enum : std::uint16_t {
    Undef = 0,
    Int32Default,
    Float32Default,
    BF16Default,
    Unknown = std::numeric_limits<std::underlying_type_t<Enum>>::max()
  };
};

} // namespace franklin

#endif // FRANKLIN_CORE_DATA_TYPE_ENUM_HPP
