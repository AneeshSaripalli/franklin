#ifndef FRANKLIN_CORE_DATA_TYPE_ENUM_HPP
#define FRANKLIN_CORE_DATA_TYPE_ENUM_HPP

#include <cstdint>

namespace franklin {

class DataTypeEnum {
public:
  enum Enum : std::uint16_t {
    Int32Default = 0,
    Float32Default = 1,
    BF16Default = 2,
    // Add more as needed
  };
};

} // namespace franklin

#endif // FRANKLIN_CORE_DATA_TYPE_ENUM_HPP
