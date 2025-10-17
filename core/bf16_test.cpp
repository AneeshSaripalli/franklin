#include "core/bf16.hpp"
#include <bit>
#include <cmath>
#include <gtest/gtest.h>
#include <immintrin.h>

namespace franklin {
namespace {

TEST(BF16Test, SizeIsCorrect) {
  EXPECT_EQ(sizeof(bf16), 2);
}

TEST(BF16Test, DefaultConstructorIsZero) {
  bf16 val;
  EXPECT_EQ(val.to_bits(), 0);
  EXPECT_EQ(val.to_float(), 0.0f);
}

TEST(BF16Test, BitLayoutIsCorrect) {
  // Test bit layout: sign (bit 15), exponent (bits 14-7), mantissa (bits 6-0)

  // Test: positive zero (all bits 0)
  bf16 pos_zero = bf16::from_bits(0x0000);
  EXPECT_EQ(pos_zero.sign, 0);
  EXPECT_EQ(pos_zero.exponent, 0);
  EXPECT_EQ(pos_zero.mantissa, 0);

  // Test: negative zero (only sign bit set)
  bf16 neg_zero = bf16::from_bits(0x8000);
  EXPECT_EQ(neg_zero.sign, 1);
  EXPECT_EQ(neg_zero.exponent, 0);
  EXPECT_EQ(neg_zero.mantissa, 0);

  // Test: value with all fields set
  // Bits: 1 (sign) | 10000000 (exp=128) | 1111111 (mantissa=127)
  // = 0b1100000001111111 = 0xC07F
  bf16 test_val = bf16::from_bits(0xC07F);
  EXPECT_EQ(test_val.sign, 1);
  EXPECT_EQ(test_val.exponent, 128);
  EXPECT_EQ(test_val.mantissa, 127);

  // Verify round-trip
  EXPECT_EQ(test_val.to_bits(), 0xC07F);
}

TEST(BF16Test, FloatConversionSimpleValues) {
  // Test 1.0f
  // Float32: 0x3F800000 (0 | 01111111 | 00000000000000000000000)
  // BF16:    0x3F80     (0 | 01111111 | 0000000)
  bf16 one = bf16::from_float(1.0f);
  EXPECT_EQ(one.to_bits(), 0x3F80);
  EXPECT_EQ(one.to_float(), 1.0f);

  // Test -1.0f
  // Float32: 0xBF800000 (1 | 01111111 | 00000000000000000000000)
  // BF16:    0xBF80     (1 | 01111111 | 0000000)
  bf16 neg_one = bf16::from_float(-1.0f);
  EXPECT_EQ(neg_one.to_bits(), 0xBF80);
  EXPECT_EQ(neg_one.to_float(), -1.0f);

  // Test 2.0f
  // Float32: 0x40000000 (0 | 10000000 | 00000000000000000000000)
  // BF16:    0x4000     (0 | 10000000 | 0000000)
  bf16 two = bf16::from_float(2.0f);
  EXPECT_EQ(two.to_bits(), 0x4000);
  EXPECT_EQ(two.to_float(), 2.0f);
}

TEST(BF16Test, ConversionViaConstructor) {
  bf16 val(3.14159f);
  float result = static_cast<float>(val);

  // BF16 has lower precision, so we expect some loss
  // 3.14159 in BF16 should be close but not exact
  EXPECT_NEAR(result, 3.14159f, 0.01f);
}

TEST(BF16Test, SpecialValues) {
  // Test infinity
  bf16 pos_inf = bf16::from_float(INFINITY);
  EXPECT_TRUE(std::isinf(pos_inf.to_float()));
  EXPECT_GT(pos_inf.to_float(), 0.0f);

  bf16 neg_inf = bf16::from_float(-INFINITY);
  EXPECT_TRUE(std::isinf(neg_inf.to_float()));
  EXPECT_LT(neg_inf.to_float(), 0.0f);

  // Test NaN
  bf16 nan_val = bf16::from_float(NAN);
  EXPECT_TRUE(std::isnan(nan_val.to_float()));
}

TEST(BF16Test, SignBitLocation) {
  // Verify sign is in bit 15
  bf16 positive = bf16::from_bits(0x3F80); // 1.0f
  bf16 negative = bf16::from_bits(0xBF80); // -1.0f

  EXPECT_EQ(positive.sign, 0);
  EXPECT_EQ(negative.sign, 1);

  // Only difference should be bit 15
  EXPECT_EQ(positive.to_bits() ^ negative.to_bits(), 0x8000);
}

TEST(BF16Test, ExponentBitRange) {
  // Exponent should be bits 14-7
  // Max exponent value: 255 (all 8 bits set)
  bf16 max_exp = bf16::from_bits(0x7F80); // 0 | 11111111 | 0000000
  EXPECT_EQ(max_exp.exponent, 255);
  EXPECT_EQ(max_exp.sign, 0);
  EXPECT_EQ(max_exp.mantissa, 0);
}

TEST(BF16Test, MantissaBitRange) {
  // Mantissa should be bits 6-0
  // Max mantissa value: 127 (all 7 bits set)
  bf16 max_mantissa = bf16::from_bits(0x007F); // 0 | 00000000 | 1111111
  EXPECT_EQ(max_mantissa.mantissa, 127);
  EXPECT_EQ(max_mantissa.exponent, 0);
  EXPECT_EQ(max_mantissa.sign, 0);
}

TEST(BF16Test, TiesToEvenRounding) {
  // Test that _mm256_cvtneps_pbh implements round-to-nearest-ties-to-even
  // When fp32 mantissa bits 7-23 are exactly 10000000000000000 (a tie),
  // the value is exactly halfway between two bf16 representable values.
  // IEEE 754 ties-to-even: round to the nearest value with even LSB.

  // Helper to convert fp32 bits to bf16 using native AVX instruction
  auto convert_with_avx = [](std::uint32_t fp32_bits) -> std::uint16_t {
    float fp32_val = std::bit_cast<float>(fp32_bits);

    // Create __m256 with 8 copies of the same value
    __m256 vec = _mm256_set1_ps(fp32_val);

    // Convert to bf16 using native instruction (with rounding)
    __m128bh bf16_vec = _mm256_cvtneps_pbh(vec);

    // Extract the first bf16 value
    __m128i bf16_as_int = reinterpret_cast<__m128i>(bf16_vec);
    std::uint16_t result = _mm_extract_epi16(bf16_as_int, 0);

    return result;
  };

  // Test Case 1: Mantissa overflow tie → round UP to next exponent (even LSB)
  // fp32 mantissa: 1111111_1000000000000000 (binary)
  // Truncated BF16 mantissa: 1111111 (127, odd LSB)
  // This is exactly halfway between 1.1111111 and 1.10000000 (which
  // becomes 2.0) Ties-to-even: round to nearest even LSB The two representable
  // values are:
  //   - 1.1111111 (mantissa=1111111, odd LSB)
  //   - 1.0 × 2^1 (mantissa=0000000, even LSB, exponent incremented)
  // Round to even LSB means round UP to exponent=128, mantissa=0
  //
  // Float32 layout: sign(1) | exponent(8) | mantissa(23)
  // Using exponent=127 (biased, represents 2^0 = 1.0 range)
  // Bits: 0 | 01111111 | 11111111000000000000000
  {
    std::uint32_t fp32_bits = 0b0'01111111'11111111000000000000000;
    std::uint16_t bf16_bits = convert_with_avx(fp32_bits);

    // Expected: round up to next exponent (mantissa overflow)
    // BF16: 0 | 10000000 | 0000000 (exponent=128, mantissa=0, even LSB)
    std::uint16_t expected = 0b0'10000000'0000000;

    EXPECT_EQ(bf16_bits, expected)
        << "Mantissa all-1s + tie should round UP to next exponent (even LSB). "
           "Got 0x"
        << std::hex << bf16_bits << " expected 0x" << expected;
  }

  // Test Case 2: Even LSB + tie → round DOWN (stay even)
  // fp32 mantissa: 1111110_1000000000000000 (binary)
  // BF16 mantissa LSB is 0 (even), so stay at 1111110 (126, even)
  // Bits: 0 | 01111111 | 11111101000000000000000
  {
    std::uint32_t fp32_bits = 0b0'01111111'11111101000000000000000;
    std::uint16_t bf16_bits = convert_with_avx(fp32_bits);

    // Expected: stay even (round down)
    // BF16: 0 | 01111111 | 1111110
    std::uint16_t expected = 0b0'01111111'1111110;

    EXPECT_EQ(bf16_bits, expected)
        << "Even LSB + tie should stay even. Got 0x" << std::hex << bf16_bits
        << " expected 0x" << expected;
  }

  // Test Case 3: Different odd LSB + tie → round UP to even
  // fp32 mantissa: 0000001_1000000000000000 (binary)
  // BF16 mantissa LSB is 1 (odd), so round up to 0000010 (2, even)
  // Bits: 0 | 01111111 | 00000011000000000000000
  {
    std::uint32_t fp32_bits = 0b0'01111111'00000011000000000000000;
    std::uint16_t bf16_bits = convert_with_avx(fp32_bits);

    // Expected: round up to even
    // BF16: 0 | 01111111 | 0000010
    std::uint16_t expected = 0b0'01111111'0000010;

    EXPECT_EQ(bf16_bits, expected)
        << "Odd LSB + tie should round UP to even. Got 0x" << std::hex
        << bf16_bits << " expected 0x" << expected;
  }

  // Test Case 4: Even LSB + tie → round DOWN (stay even)
  // fp32 mantissa: 0000010_1000000000000000 (binary)
  // Truncated: 0000010 (LSB=0, even)
  // Halfway between 0000010 (even) and 0000011 (odd)
  // Ties-to-even: stay at 0000010 (even)
  // Bits: 0 | 01111111 | 00000101000000000000000
  {
    std::uint32_t fp32_bits = 0b0'01111111'00000101000000000000000;
    std::uint16_t bf16_bits = convert_with_avx(fp32_bits);

    // Expected: stay at even value (round down)
    // BF16: 0 | 01111111 | 0000010
    std::uint16_t expected = 0b0'01111111'0000010;

    EXPECT_EQ(bf16_bits, expected)
        << "Even LSB + tie should stay even (round down). Got 0x" << std::hex
        << bf16_bits << " expected 0x" << expected;
  }

  // Test Case 5: Non-tie, round down (bits 7-23 < 0x8000)
  // fp32 mantissa: 1111111_0100000000000000 (less than halfway)
  // Should truncate down to 1111111
  {
    std::uint32_t fp32_bits = 0b0'01111111'11111110100000000000000;
    std::uint16_t bf16_bits = convert_with_avx(fp32_bits);

    // Expected: round down
    // BF16: 0 | 01111111 | 1111111
    std::uint16_t expected = 0b0'01111111'1111111;

    EXPECT_EQ(bf16_bits, expected)
        << "Non-tie (< halfway) should round down. Got 0x" << std::hex
        << bf16_bits << " expected 0x" << expected;
  }

  // Test Case 6: Non-tie, round up (bits 7-23 > 0x8000)
  // fp32 mantissa: 1111111_1100000000000000 (more than halfway)
  // Should round up to 10000000 (overflow to next value)
  {
    std::uint32_t fp32_bits = 0b0'01111111'11111111100000000000000;
    std::uint16_t bf16_bits = convert_with_avx(fp32_bits);

    // Expected: round up (note: might overflow mantissa and increment exponent)
    // BF16: 0 | 10000000 | 0000000 (mantissa overflow increments exponent)
    std::uint16_t expected = 0b0'10000000'0000000;

    EXPECT_EQ(bf16_bits, expected)
        << "Non-tie (> halfway) should round up. Got 0x" << std::hex
        << bf16_bits << " expected 0x" << expected;
  }
}

} // namespace
} // namespace franklin
