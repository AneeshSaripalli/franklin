#pragma once

#include <bit>
#include <cstdint>

namespace franklin {

// Brain Float 16-bit floating point format
// Layout: 1 sign bit, 8 exponent bits, 7 mantissa bits
// Compatible with truncated IEEE 754 single precision (float32)
struct bf16 {
  // Bitfield layout (reversed for little-endian memory ordering)
  // This ensures: bit 15 = sign, bits 14-7 = exponent, bits 6-0 = mantissa
  std::uint16_t mantissa : 7;
  std::uint16_t exponent : 8;
  std::uint16_t sign : 1;

  // Default constructor - zero value
  constexpr bf16() noexcept : mantissa(0), exponent(0), sign(0) {}

  // Construct from raw bits
  static constexpr bf16 from_bits(std::uint16_t bits) noexcept {
    return std::bit_cast<bf16>(bits);
  }

  // Get raw bits
  constexpr std::uint16_t to_bits() const noexcept {
    return std::bit_cast<std::uint16_t>(*this);
  }

  // Convert from float32 (truncation method)
  static bf16 from_float(float value) noexcept {
    std::uint32_t bits = std::bit_cast<std::uint32_t>(value);

    // BF16 is just the top 16 bits of float32
    // Truncate (no rounding for now)
    std::uint16_t bf16_bits = static_cast<std::uint16_t>(bits >> 16);

    return from_bits(bf16_bits);
  }

  // Convert to float32
  float to_float() const noexcept {
    // BF16 to float32: shift left 16 bits and add zero lower bits
    std::uint32_t bits = static_cast<std::uint32_t>(to_bits()) << 16;
    return std::bit_cast<float>(bits);
  }

  // Conversion operators
  explicit operator float() const noexcept { return to_float(); }

  // Construct from float
  explicit bf16(float value) noexcept : bf16(from_float(value)) {}
};

static_assert(sizeof(bf16) == sizeof(std::uint16_t),
              "bf16 must be exactly 16 bits");

} // namespace franklin
