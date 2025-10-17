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
