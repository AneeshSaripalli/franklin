# Franklin Python Bindings

**Simple Python interface using CFFI ABI mode - no compilation needed!**

## Quick Start

```bash
# 1. Build the C++ shared library
bazel build //python:libfranklin_c_api.so

# 2. Run Python (venv already has cffi installed)
cd python
.venv/bin/python hello_world.py
```

Done! No extra build steps.

## What's in This Directory

```
python/
├── franklin_c_api.{h,cpp}   # C++ wrapper (C ABI, builds .so)
├── BUILD                     # Bazel: builds libfranklin_c_api.so
├── franklin.py               # Python CFFI wrapper (loads .so)
├── hello_world.py            # Example
├── run.sh                    # Helper: ./run.sh hello_world.py
├── requirements.txt          # Just: cffi>=1.15.0
└── .venv/                    # Python venv (uv-managed, cffi installed)
```

## How It Works

1. **C++ → C ABI** (`franklin_c_api.cpp`):
   - Wraps `column_vector`, `interpreter` with C functions
   - Built as `libfranklin_c_api.so` by Bazel

2. **CFFI loads .so** (`franklin.py`):
   - Uses **ABI mode** (just loads binary, no compilation)
   - Declares C function signatures
   - Provides Pythonic API

3. **Zero Python compilation**:
   - Old `build_cffi.py` (API mode) was overcomplicated
   - This is simpler: just load the pre-built .so

## Example

```python
import franklin

# Create interpreter
interp = franklin.Interpreter(dtype="int32")

# Create columns
a = franklin.Column.create("int32", size=5, value=10)
b = franklin.Column.create("int32", size=5, value=20)

# Register with names
interp.register("a", a)
interp.register("b", b)

# Evaluate
result = interp.eval("a")
print(result.to_list())  # [10, 10, 10, 10, 10]
```

## Virtual Environment Setup

Already done with `uv`:
```bash
~/.local/bin/uv venv           # Created .venv/
~/.local/bin/uv pip install cffi  # Installed cffi 2.0.0
```

To recreate: `cd python && uv venv && uv pip install -r requirements.txt`

## Next Steps

- **Binary operations** (`a + b`) need implementation in `core/interpreter.hpp`
- **Full expression system** - see `plans/JIT.md` for design

## Why This Is Better

**Old approach (what we deleted):**
- `build_cffi.py` - complex API-mode setup
- Compile Python C extensions
- Platform-specific `.so` files
- Extra build step for users

**New approach:**
- Simple ABI mode
- Just load pre-built .so
- One less thing to compile
- Works immediately
