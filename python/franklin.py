"""
Franklin Python bindings using CFFI ABI mode (simple, no compilation needed).
"""

from cffi import FFI
import os

# Create FFI instance
ffi = FFI()

# Load C declarations from header file


def load_c_declarations(header_path):
    """Read C header and strip preprocessor directives for CFFI."""
    with open(header_path) as f:
        lines = []
        for line in f:
            stripped = line.strip()
            # Skip preprocessor directives and empty lines
            if (stripped.startswith('#') or
                stripped.startswith('extern "C"') or
                stripped == '{' or stripped == '}' or
                    not stripped):
                continue
            lines.append(line.rstrip())
        return '\n'.join(lines)


# Declare the C API (ABI mode - just signatures, no compilation)
header_path = os.path.join(os.path.dirname(__file__), 'franklin_c_api.h')
ffi.cdef(load_c_declarations(header_path))

# Find and load the shared library
# Try multiple locations
lib_paths = [
    "../bazel-bin/python/libfranklin_c_api.so",
    "./bazel-bin/python/libfranklin_c_api.so",
    "bazel-bin/python/libfranklin_c_api.so",
]

lib = None
for path in lib_paths:
    if os.path.exists(path):
        lib = ffi.dlopen(path)
        break

if lib is None:
    raise RuntimeError(
        f"Could not find libfranklin_c_api.so. Tried: {lib_paths}\n"
        "Did you run: bazel build //python:libfranklin_c_api.so ?"
    )


# High-level Python wrapper
class DataType:
    """Franklin data types."""
    INT32 = 0
    FLOAT32 = 1


class Column:
    """Wrapper for Franklin column."""

    def __init__(self, handle):
        self._handle = handle
        self._owned = True

    @classmethod
    def create(cls, dtype, size, value=0):
        """
        Create a column.

        Args:
            dtype: "int32" or "float32" or DataType.INT32/FLOAT32
            size: Number of elements
            value: Initial value for all elements
        """
        if isinstance(dtype, str):
            dtype_map = {"int32": DataType.INT32, "float32": DataType.FLOAT32}
            if dtype not in dtype_map:
                raise ValueError(f"Unknown dtype: {dtype}")
            dtype = dtype_map[dtype]

        handle = lib.franklin_column_create(dtype, size, float(value))
        if handle == ffi.NULL:
            raise RuntimeError(f"Failed to create column with dtype={dtype}")
        return cls(handle)

    def __del__(self):
        if self._owned and self._handle != ffi.NULL:
            lib.franklin_column_destroy(self._handle)
            self._handle = ffi.NULL

    def __len__(self):
        return lib.franklin_column_size(self._handle)

    def __getitem__(self, index):
        if index < 0 or index >= len(self):
            raise IndexError(
                f"Column index {index} out of range [0, {
                    len(self)})")
        return lib.franklin_column_get_int32(self._handle, index)

    def to_list(self):
        return [self[i] for i in range(len(self))]

    def _release(self):
        """Release ownership (for passing to interpreter)."""
        self._owned = False
        handle = self._handle
        self._handle = ffi.NULL
        return handle


class Interpreter:
    """Wrapper for Franklin interpreter."""

    def __init__(self, dtype="int32"):
        if dtype == "int32":
            self._handle = lib.franklin_interpreter_create_int32()
        else:
            raise ValueError(f"Unsupported dtype: {dtype}")

        if self._handle == ffi.NULL:
            raise RuntimeError("Failed to create interpreter")

    def __del__(self):
        if hasattr(self, '_handle') and self._handle != ffi.NULL:
            lib.franklin_interpreter_destroy(self._handle)
            self._handle = ffi.NULL

    def register(self, name, column):
        """Register a column (takes ownership)."""
        handle = column._release()
        success = lib.franklin_interpreter_register(
            self._handle, name.encode('utf-8'), handle
        )
        if not success:
            raise RuntimeError(f"Failed to register column '{name}'")

    def eval(self, expression):
        """Evaluate an expression."""
        result_handle = lib.franklin_interpreter_eval(
            self._handle, expression.encode('utf-8')
        )
        if result_handle == ffi.NULL:
            raise RuntimeError(
                f"Failed to evaluate expression: '{expression}'")
        return Column(result_handle)

    def has_column(self, name):
        """Check if column is registered."""
        return lib.franklin_interpreter_has_column(
            self._handle, name.encode('utf-8')
        )

    def __len__(self):
        return lib.franklin_interpreter_size(self._handle)


def version():
    """Get Franklin version."""
    return ffi.string(lib.franklin_version()).decode('utf-8')
