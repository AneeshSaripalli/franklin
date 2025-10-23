#ifndef FRANKLIN_CORE_DATA_TYPE_ENUM_HPP
#define FRANKLIN_CORE_DATA_TYPE_ENUM_HPP

#include <cstdint>
#include <limits>
#include <string_view>
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

  constexpr static std::string_view to_string(Enum e) noexcept {
    using namespace std::string_view_literals;
    switch (e) {
    case Undef:
      return "Undef"sv;
    case Int32Default:
      return "Int32Default"sv;
    case Float32Default:
      return "Float32Default"sv;
    case BF16Default:
      return "BF16Default"sv;
    case Unknown:
      [[fallthrough]];
    default:
      return "Unknown";
    }
  }
};

} // namespace franklin

#endif // FRANKLIN_CORE_DATA_TYPE_ENUM_HPP
