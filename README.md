# Franklin

A high-performance C++20 data structures library designed for computational efficiency and hardware-aware optimization.

## Overview

Franklin is a modern C++ library focused on providing high-performance implementations of fundamental data structures and numeric types, with particular emphasis on:

- **SIMD-accelerated operations** using AVX/AVX-512 intrinsics
- **Cache-friendly memory layouts** with policy-based design
- **Zero-cost abstractions** through compile-time optimization
- **Hardware-aware algorithms** optimized for modern CPU architectures

## Features

### Core Data Types

- **BF16 (Brain Float 16)**: IEEE 754-compliant 16-bit floating point type with native AVX-512 conversion support
- **Column Vectors**: SIMD-accelerated columnar storage with pluggable pipeline policies for type transformations
- **Dynamic Bitsets**: High-performance bit manipulation with optimized bulk operations
- **Buddy Allocator**: Memory allocator with power-of-two size classes

### Performance Characteristics

- **SIMD Vectorization**: Automatic vectorization for int32 and bf16 arithmetic operations
- **Pipeline Architecture**: Separates storage format from compute format (e.g., bf16 → fp32 → compute → bf16)
- **Policy-Based Design**: Compile-time customization without runtime overhead
- **Constant-Time Operations**: Critical path operations designed for O(1) complexity

## Requirements

- **C++ Compiler**: GCC 13+ or Clang 16+ with C++20 support
- **Build System**: Bazel 8.4.2+
- **CPU**: x86-64 with AVX2 support (AVX-512 BF16 for bfloat16 operations)
- **OS**: Linux (tested on Ubuntu 22.04+)

## Building

Franklin uses [Bazel](https://bazel.build/) with Bzlmod for dependency management.

### Quick Start

```bash
# Build all targets
bazel build //...

# Run all tests
bazel test //...

# Build with optimizations
bazel build -c opt //...

# Run benchmarks
bazel test //benchmarks:... --test_tag_filters=benchmark
```

### Build Configurations

- **Debug** (`-c dbg`): `-O0 -g` with frame pointers for debugging
- **Optimized** (`-c opt`): `-O3 -march=native -ffast-math` for performance
- **Default**: `-O2` standard build

## Project Structure

```
franklin/
├── core/              # Fundamental types and utilities
│   ├── bf16.hpp       # Brain Float 16 implementation
│   ├── compiler_macros.hpp
│   ├── error_collector.hpp
│   └── matrix.hpp
├── container/         # Data structures
│   ├── column.hpp     # SIMD column vectors
│   ├── dynamic_bitset.hpp
│   └── interpreter.hpp
├── memory/            # Memory management
│   ├── buddy_allocator.hpp
│   └── buddy_allocator_adapter.hpp
├── benchmarks/        # Performance benchmarks
├── examples/          # Example usage
└── error_reports/     # Bug tracking and test reports
```

## Components

### BF16 (Brain Float 16)

IEEE 754-compliant 16-bit floating point type compatible with Google's bfloat16 format.

**Layout**: 1 sign bit, 8 exponent bits, 7 mantissa bits

**Features**:
- Native AVX-512 BF16 conversion using `_mm256_cvtpbh_ps` and `_mm256_cvtneps_pbh`
- IEEE 754 round-to-nearest-ties-to-even rounding
- Constexpr-compatible bitwise operations
- Zero-overhead conversions to/from float32

**Usage**:
```cpp
#include "core/bf16.hpp"

franklin::bf16 x(3.14159f);
float y = static_cast<float>(x);  // Round-trip conversion
```

### Column Vectors

SIMD-accelerated columnar storage with type transformation pipelines.

**Features**:
- Policy-based design for compile-time customization
- Pluggable pipeline policies (Int32Pipeline, BF16Pipeline)
- Automatic buffer reuse with rvalue references
- Missing value tracking with constant-time `present()` API

**Pipeline Architecture**:
```
load → transform_to → op → transform_from → store
```

For BF16: bf16 → fp32 → add → bf16 (using native instructions)

**Usage**:
```cpp
#include "container/column.hpp"

using Column = franklin::column_vector<Int32ColumnPolicy>;
Column a(1000);
Column b(1000);
Column c = a + b;  // SIMD-vectorized addition
```

### Dynamic Bitset

High-performance bit manipulation with optimized bulk operations.

**Features**:
- 64-bit block storage with bit-level indexing
- SIMD-accelerated bulk operations
- Error collection framework for production debugging
- Optimized variants for different use cases

## Performance

Franklin is designed for high-performance computing workloads:

- **Column Vector Addition**: ~8 elements per iteration with AVX2
- **BF16 Operations**: Native hardware acceleration on AVX-512 CPUs
- **Bitset Operations**: Cache-line aligned block operations
- **Zero Allocations**: Rvalue reference optimizations for buffer reuse

## Testing

Comprehensive test suite with >100 tests covering:

- Unit tests (GoogleTest)
- Performance benchmarks (Google Benchmark)
- Stress tests for edge cases
- IEEE 754 compliance tests

```bash
# Run all tests
bazel test //...

# Run specific module tests
bazel test //core:bf16_test
bazel test //container:column_test

# Run with detailed output
bazel test //... --test_output=all
```

## Development

### Code Quality

- **C++20 Standard**: Modern C++ with concepts, ranges, and constexpr
- **Clang Format**: Automated formatting (`.clang-format`)
- **ClangD**: Language server configuration (`.clangd`)
- **Compile Commands**: Generated for IDE integration

### Compiler Flags

- `-std=c++20`: C++20 standard
- `-march=native`: CPU-specific optimizations
- `-ffast-math`: Aggressive floating-point optimizations (opt builds)

### Debug Assertions

Debug-only validation using `FRANKLIN_DEBUG_ASSERT`:
```cpp
FRANKLIN_DEBUG_ASSERT(index < size && "Index out of bounds");
```

Enabled in debug builds, compiled out in optimized/production builds.

## License

[License information to be added]

## Contributing

[Contributing guidelines to be added]

## Acknowledgments

Built with:
- [Bazel](https://bazel.build/) - Build system
- [GoogleTest](https://github.com/google/googletest) - Testing framework
- [Google Benchmark](https://github.com/google/benchmark) - Performance benchmarking
