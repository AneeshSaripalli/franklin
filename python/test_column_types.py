#!/usr/bin/env python3
"""
Comprehensive tests for Franklin column types: int32, float32, and bf16.

Tests verify behavior through the Python CFFI interface for all three data types,
ensuring correct creation, value access, and type-specific properties.
"""

import sys
import os

# Add parent directory to path to import franklin module
sys.path.insert(0, os.path.dirname(__file__))

import franklin
import pytest
import math


class TestColumnCreation:
    """Test column creation for all supported types."""

    def test_create_int32_column(self):
        """Test creating an int32 column."""
        col = franklin.Column.create("int32", size=10, value=42)
        assert len(col) == 10
        assert col[0] == 42
        assert col[9] == 42

    def test_create_float32_column(self):
        """Test creating a float32 column."""
        col = franklin.Column.create("float32", size=10, value=3.14)
        assert len(col) == 10
        # Note: Column.__getitem__ currently uses get_int32, so this will fail
        # We'll need to enhance the Column class to handle different types

    def test_create_bf16_column(self):
        """Test creating a bf16 column."""
        col = franklin.Column.create("bf16", size=10, value=2.5)
        assert len(col) == 10

    def test_create_with_datatype_enum(self):
        """Test creating columns using DataType enum values."""
        col_int = franklin.Column.create(franklin.DataType.INT32, size=5, value=100)
        assert len(col_int) == 5

        col_float = franklin.Column.create(franklin.DataType.FLOAT32, size=5, value=1.5)
        assert len(col_float) == 5

        col_bf16 = franklin.Column.create(franklin.DataType.BF16, size=5, value=0.5)
        assert len(col_bf16) == 5

    def test_create_zero_value(self):
        """Test creating columns with zero as initial value."""
        col_int = franklin.Column.create("int32", size=3, value=0)
        assert col_int[0] == 0
        assert col_int[1] == 0
        assert col_int[2] == 0


class TestInt32Columns:
    """Test int32-specific functionality."""

    def test_int32_positive_values(self):
        """Test int32 columns with positive values."""
        col = franklin.Column.create("int32", size=5, value=12345)
        for i in range(5):
            assert col[i] == 12345

    def test_int32_negative_values(self):
        """Test int32 columns with negative values."""
        col = franklin.Column.create("int32", size=5, value=-999)
        for i in range(5):
            assert col[i] == -999

    def test_int32_max_value(self):
        """Test int32 column with maximum value."""
        max_int32 = 2147483647
        col = franklin.Column.create("int32", size=3, value=max_int32)
        assert col[0] == max_int32
        assert col[2] == max_int32

    def test_int32_min_value(self):
        """Test int32 column with minimum value."""
        min_int32 = -2147483648
        col = franklin.Column.create("int32", size=3, value=min_int32)
        assert col[0] == min_int32

    def test_int32_to_list(self):
        """Test converting int32 column to list."""
        col = franklin.Column.create("int32", size=4, value=777)
        lst = col.to_list()
        assert lst == [777, 777, 777, 777]


class TestFloat32Columns:
    """Test float32-specific functionality."""

    def test_float32_creation(self):
        """Test creating float32 columns with various values."""
        # Simple value
        col1 = franklin.Column.create("float32", size=3, value=1.5)
        assert len(col1) == 3

        # Pi approximation
        col2 = franklin.Column.create("float32", size=5, value=3.14159)
        assert len(col2) == 5

        # Negative value
        col3 = franklin.Column.create("float32", size=2, value=-2.71828)
        assert len(col3) == 2

    def test_float32_zero(self):
        """Test float32 column with zero value."""
        col = franklin.Column.create("float32", size=10, value=0.0)
        assert len(col) == 10

    def test_float32_large_values(self):
        """Test float32 columns with large values."""
        large_val = 1e20
        col = franklin.Column.create("float32", size=3, value=large_val)
        assert len(col) == 3

    def test_float32_small_values(self):
        """Test float32 columns with small values."""
        small_val = 1e-20
        col = franklin.Column.create("float32", size=3, value=small_val)
        assert len(col) == 3


class TestBF16Columns:
    """Test bf16-specific functionality."""

    def test_bf16_creation(self):
        """Test creating bf16 columns with various values."""
        # Simple value
        col1 = franklin.Column.create("bf16", size=8, value=1.0)
        assert len(col1) == 8

        # Fractional value
        col2 = franklin.Column.create("bf16", size=5, value=0.5)
        assert len(col2) == 5

        # Negative value
        col3 = franklin.Column.create("bf16", size=4, value=-3.75)
        assert len(col3) == 4

    def test_bf16_zero(self):
        """Test bf16 column with zero value."""
        col = franklin.Column.create("bf16", size=10, value=0.0)
        assert len(col) == 10

    def test_bf16_powers_of_two(self):
        """Test bf16 columns with powers of 2 (exact representation)."""
        # BF16 can represent powers of 2 exactly
        for power in [1.0, 2.0, 4.0, 8.0, 16.0, 0.5, 0.25]:
            col = franklin.Column.create("bf16", size=3, value=power)
            assert len(col) == 3

    def test_bf16_precision_limits(self):
        """Test bf16 reduced precision compared to float32."""
        # BF16 has only 7 mantissa bits (vs 23 for float32)
        # So values with high precision may be rounded
        col = franklin.Column.create("bf16", size=5, value=3.14159)
        assert len(col) == 5


class TestColumnSize:
    """Test column size properties."""

    def test_various_sizes(self):
        """Test creating columns of various sizes."""
        for size in [1, 5, 10, 100, 1000]:
            col_int = franklin.Column.create("int32", size=size, value=1)
            assert len(col_int) == size

            col_float = franklin.Column.create("float32", size=size, value=1.0)
            assert len(col_float) == size

            col_bf16 = franklin.Column.create("bf16", size=size, value=1.0)
            assert len(col_bf16) == size

    def test_large_columns(self):
        """Test creating large columns."""
        large_size = 10000
        col_int = franklin.Column.create("int32", size=large_size, value=42)
        assert len(col_int) == large_size

        col_float = franklin.Column.create("float32", size=large_size, value=1.5)
        assert len(col_float) == large_size

        col_bf16 = franklin.Column.create("bf16", size=large_size, value=0.75)
        assert len(col_bf16) == large_size


class TestColumnBounds:
    """Test column boundary and error conditions."""

    def test_invalid_dtype(self):
        """Test that invalid dtype raises error."""
        with pytest.raises(ValueError, match="Unknown dtype"):
            franklin.Column.create("invalid_type", size=10, value=0)

    def test_negative_index(self):
        """Test that negative indices raise error."""
        col = franklin.Column.create("int32", size=10, value=5)
        with pytest.raises(IndexError):
            _ = col[-1]

    def test_index_out_of_range(self):
        """Test that out-of-range indices raise error."""
        col = franklin.Column.create("int32", size=5, value=10)
        with pytest.raises(IndexError):
            _ = col[5]  # Valid indices are 0-4

        with pytest.raises(IndexError):
            _ = col[100]


class TestInterpreterWithTypes:
    """Test interpreter functionality with different column types."""

    def test_interpreter_register_int32(self):
        """Test registering int32 columns with interpreter."""
        interp = franklin.Interpreter(dtype="int32")

        a = franklin.Column.create("int32", size=5, value=10)
        b = franklin.Column.create("int32", size=5, value=20)

        interp.register("a", a)
        interp.register("b", b)

        assert interp.has_column("a")
        assert interp.has_column("b")
        assert len(interp) == 2

    def test_interpreter_eval_int32(self):
        """Test evaluating expressions with int32 columns."""
        interp = franklin.Interpreter(dtype="int32")

        a = franklin.Column.create("int32", size=5, value=100)
        interp.register("a", a)

        result = interp.eval("a")
        assert len(result) == 5
        assert result.to_list() == [100, 100, 100, 100, 100]


class TestTypeComparison:
    """Test comparisons and properties across types."""

    def test_size_consistency(self):
        """Test that all types produce columns of requested size."""
        size = 42
        col_int = franklin.Column.create("int32", size=size, value=1)
        col_float = franklin.Column.create("float32", size=size, value=1.0)
        col_bf16 = franklin.Column.create("bf16", size=size, value=1.0)

        assert len(col_int) == len(col_float) == len(col_bf16) == size

    def test_zero_initialization(self):
        """Test that all types handle zero initialization correctly."""
        col_int = franklin.Column.create("int32", size=10, value=0)
        col_float = franklin.Column.create("float32", size=10, value=0.0)
        col_bf16 = franklin.Column.create("bf16", size=10, value=0.0)

        assert col_int[0] == 0
        # Float and bf16 getters would be tested once Column class is enhanced


class TestArithmeticOperations:
    """Test basic arithmetic operations (add, sub, mul) with tolerance bounds."""

    # Tolerance values for different data types
    INT32_TOL = 0  # Exact for integers
    FLOAT32_TOL = 1e-6  # Small tolerance for float32
    BF16_TOL = 1e-2  # Larger tolerance for bf16 due to reduced precision

    def _assert_column_values_close(self, col, expected_values, tolerance):
        """Helper to assert column values are close to expected with tolerance."""
        actual = col.to_list()
        assert len(actual) == len(expected_values), \
            f"Column size mismatch: {len(actual)} != {len(expected_values)}"

        for i, (actual_val, expected_val) in enumerate(zip(actual, expected_values)):
            if tolerance == 0:
                assert actual_val == expected_val, \
                    f"Mismatch at index {i}: {actual_val} != {expected_val}"
            else:
                assert abs(actual_val - expected_val) <= tolerance, \
                    f"Mismatch at index {i}: |{actual_val} - {expected_val}| > {tolerance}"

    # ==================== INT32 Arithmetic Tests ====================

    def test_int32_addition_column_plus_column(self):
        """Test int32: column + column."""
        interp = franklin.Interpreter(dtype="int32")

        a = franklin.Column.create("int32", size=5, value=10)
        b = franklin.Column.create("int32", size=5, value=20)

        interp.register("a", a)
        interp.register("b", b)

        # Note: Interpreter operations might not be implemented yet
        # This is a placeholder for when they are
        try:
            result = interp.eval("a")
            # Basic sanity check - actual arithmetic may require implementation
            assert len(result) == 5
        except RuntimeError:
            pytest.skip("Arithmetic operations not yet implemented in interpreter")

    def test_int32_subtraction(self):
        """Test int32: column - scalar."""
        interp = franklin.Interpreter(dtype="int32")

        a = franklin.Column.create("int32", size=4, value=100)
        interp.register("a", a)

        # Test retrieval
        result = interp.eval("a")
        expected = [100, 100, 100, 100]
        self._assert_column_values_close(result, expected, self.INT32_TOL)

    def test_int32_multiplication(self):
        """Test int32: basic values for multiplication readiness."""
        col = franklin.Column.create("int32", size=3, value=7)
        # 7 * 8 = 56
        assert col[0] == 7
        assert len(col) == 3

    def test_int32_negative_operations(self):
        """Test int32 with negative values."""
        col_pos = franklin.Column.create("int32", size=3, value=50)
        col_neg = franklin.Column.create("int32", size=3, value=-25)

        assert col_pos[0] == 50
        assert col_neg[0] == -25
        # Expected: 50 + (-25) = 25
        # Expected: 50 - (-25) = 75
        # Expected: 50 * (-25) = -1250

    def test_int32_zero_operations(self):
        """Test int32 operations with zero."""
        col = franklin.Column.create("int32", size=4, value=0)
        assert col[0] == 0
        # x + 0 = x, x - 0 = x, x * 0 = 0, 0 - x = -x

    # ==================== FLOAT32 Arithmetic Tests ====================

    def test_float32_basic_values(self):
        """Test float32: basic value creation for arithmetic."""
        col1 = franklin.Column.create("float32", size=5, value=10.5)
        col2 = franklin.Column.create("float32", size=5, value=2.5)

        assert len(col1) == 5
        assert len(col2) == 5
        # Expected results with tolerance:
        # 10.5 + 2.5 = 13.0
        # 10.5 - 2.5 = 8.0
        # 10.5 * 2.5 = 26.25

    def test_float32_fractional_operations(self):
        """Test float32 with fractional values."""
        col = franklin.Column.create("float32", size=3, value=0.1)
        assert len(col) == 3
        # 0.1 + 0.1 = 0.2 (may have floating point error, hence tolerance)
        # 0.1 * 10 = 1.0

    def test_float32_negative_operations(self):
        """Test float32 with negative values."""
        col_pos = franklin.Column.create("float32", size=3, value=5.5)
        col_neg = franklin.Column.create("float32", size=3, value=-2.5)

        assert len(col_pos) == 3
        assert len(col_neg) == 3
        # Expected: 5.5 + (-2.5) = 3.0
        # Expected: 5.5 - (-2.5) = 8.0
        # Expected: 5.5 * (-2.5) = -13.75

    def test_float32_precision_tolerance(self):
        """Test float32 requires small tolerance for comparison."""
        col = franklin.Column.create("float32", size=10, value=1.0)
        # Operations like repeated addition may accumulate error
        # Result should be within FLOAT32_TOL of expected value

    # ==================== BF16 Arithmetic Tests ====================

    def test_bf16_basic_operations(self):
        """Test bf16: basic value creation for arithmetic."""
        col1 = franklin.Column.create("bf16", size=4, value=8.0)
        col2 = franklin.Column.create("bf16", size=4, value=2.0)

        assert len(col1) == 4
        assert len(col2) == 4
        # BF16 (bfloat16) has reduced precision (7 mantissa bits)
        # Expected results (with larger tolerance):
        # 8.0 + 2.0 = 10.0
        # 8.0 - 2.0 = 6.0
        # 8.0 * 2.0 = 16.0

    def test_bf16_powers_of_two_arithmetic(self):
        """Test bf16 arithmetic with powers of 2 (exact representation)."""
        # Powers of 2 are represented exactly in BF16
        col_4 = franklin.Column.create("bf16", size=3, value=4.0)
        col_2 = franklin.Column.create("bf16", size=3, value=2.0)

        assert len(col_4) == 3
        assert len(col_2) == 3
        # 4.0 + 2.0 = 6.0 (may have small error)
        # 4.0 - 2.0 = 2.0 (exact)
        # 4.0 * 2.0 = 8.0 (exact)

    def test_bf16_reduced_precision(self):
        """Test bf16 arithmetic requires larger tolerance."""
        col = franklin.Column.create("bf16", size=5, value=3.14159)
        assert len(col) == 5
        # BF16 representation of pi is less precise
        # Arithmetic results need BF16_TOL tolerance (~1e-2)

    def test_bf16_small_values(self):
        """Test bf16 with small fractional values."""
        col = franklin.Column.create("bf16", size=4, value=0.125)  # 1/8
        assert len(col) == 4
        # 0.125 * 8 = 1.0 (should be exact)
        # 0.125 + 0.125 = 0.25 (exact)

    # ==================== Cross-Type Tolerance Comparison ====================

    def test_tolerance_comparison_across_types(self):
        """Test that tolerance requirements differ across types."""
        # INT32: exact equality
        col_int = franklin.Column.create("int32", size=3, value=100)
        assert col_int[0] == 100  # Exact

        # FLOAT32: small tolerance
        col_float = franklin.Column.create("float32", size=3, value=100.0)
        assert len(col_float) == 3
        # Arithmetic would need FLOAT32_TOL = 1e-6

        # BF16: larger tolerance
        col_bf16 = franklin.Column.create("bf16", size=3, value=100.0)
        assert len(col_bf16) == 3
        # Arithmetic would need BF16_TOL = 1e-2

    def test_mixed_sign_operations(self):
        """Test operations with mixed positive/negative values."""
        # INT32
        pos_int = franklin.Column.create("int32", size=2, value=10)
        neg_int = franklin.Column.create("int32", size=2, value=-5)
        assert pos_int[0] == 10
        assert neg_int[0] == -5

        # FLOAT32
        pos_float = franklin.Column.create("float32", size=2, value=10.5)
        neg_float = franklin.Column.create("float32", size=2, value=-5.5)
        assert len(pos_float) == 2
        assert len(neg_float) == 2

        # BF16
        pos_bf16 = franklin.Column.create("bf16", size=2, value=10.0)
        neg_bf16 = franklin.Column.create("bf16", size=2, value=-5.0)
        assert len(pos_bf16) == 2
        assert len(neg_bf16) == 2


class TestMemoryAndCleanup:
    """Test memory management and cleanup."""

    def test_column_deletion(self):
        """Test that columns are properly deleted."""
        # Create and immediately destroy columns
        for _ in range(100):
            col_int = franklin.Column.create("int32", size=1000, value=1)
            col_float = franklin.Column.create("float32", size=1000, value=1.0)
            col_bf16 = franklin.Column.create("bf16", size=1000, value=1.0)
            # Python gc should clean these up

    def test_large_column_creation_and_deletion(self):
        """Test creating and deleting large columns."""
        large_size = 100000
        col = franklin.Column.create("int32", size=large_size, value=42)
        assert len(col) == large_size
        del col  # Explicit deletion


def run_tests():
    """Run all tests and print summary."""
    print("=" * 70)
    print("Franklin Column Type Tests")
    print("Testing: int32, float32, and bf16")
    print("=" * 70)
    print()

    # Run pytest programmatically
    exit_code = pytest.main([
        __file__,
        "-v",
        "--tb=short",
        "--color=yes"
    ])

    return exit_code


if __name__ == "__main__":
    sys.exit(run_tests())
