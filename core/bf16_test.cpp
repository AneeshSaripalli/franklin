#include "core/bf16.hpp"
#include <cmath>
#include <gtest/gtest.h>

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

} // namespace
} // namespace franklin
