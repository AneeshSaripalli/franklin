# AsmJit Examples

This directory contains progressive examples demonstrating AsmJit's interface and capabilities for runtime code generation.

## Building and Running

```bash
# Build a specific example
bazel build -c opt //examples:asmjit_01_basic_function

# Run it
bazel-bin/examples/asmjit_01_basic_function

# Or build and run in one command
bazel run -c opt //examples:asmjit_01_basic_function
```

## Example Progression

### 01: Basic Function Generation
**File**: `asmjit_01_basic_function.cpp`

**What it teaches**:
- Creating a JitRuntime and CodeHolder
- Using the Assembler interface
- Generating a simple scalar function: `int32_t add(int32_t a, int32_t b)`
- Understanding x86-64 calling conventions

**Key API calls**:
```cpp
asmjit::JitRuntime rt;
asmjit::CodeHolder code;
code.init(rt.environment(), rt.cpu_features());
asmjit::x86::Assembler a(&code);

a.mov(eax, edi);   // Move first arg to return register
a.add(eax, esi);   // Add second arg
a.ret();           // Return

a.finalize();
rt.add(&func_ptr, &code);
```

**Run this first** to understand the basic workflow.

---

### 02: Integer SIMD Operations
**File**: `asmjit_02_integer_simd.cpp`

**What it teaches**:
- Using AVX2 ymm registers for SIMD
- Integer vector instructions (`vpaddd`, `vmovdqu`)
- The difference between integer and float SIMD
- Memory addressing for arrays

**Key insight**: `vpaddd` preserves full 32-bit integer precision!

**Critical instructions**:
```cpp
a.vmovdqu(ymm0, ptr(rdi));      // Load 8x int32
a.vpaddd(ymm0, ymm0, ymm1);     // Integer add (NOT vaddps!)
a.vmovdqu(ptr(rdx), ymm0);      // Store 8x int32
```

---

### 03: Float SIMD Operations
**File**: `asmjit_03_float_simd.cpp`

**What it teaches**:
- Float vector instructions (`vaddps`, `vmovups`)
- When to use float vs integer SIMD
- Comparing with example 02

**Critical difference**: `vaddps` for float32, `vpaddd` for int32

**Key instructions**:
```cpp
a.vmovups(ymm0, ptr(rdi));      // Load 8x float32
a.vaddps(ymm0, ymm0, ymm1);     // Float add (NOT vpaddd!)
a.vmovups(ptr(rdx), ymm0);      // Store 8x float32
```

---

### 04: Type Conversion
**File**: `asmjit_04_type_conversion.cpp`

**What it teaches**:
- Converting int32 ↔ float32 with SIMD
- When conversions are necessary vs wasteful
- The **critical problem** with polymorphic branching

**THE BIG INSIGHT**:
```cpp
// WRONG: int32 + int32 in float domain (wasteful!)
a.vcvtdq2ps(ymm0, ymm0);  // Convert int32 -> float
a.vcvtdq2ps(ymm1, ymm1);  // Convert int32 -> float
a.vaddps(ymm0, ymm0, ymm1);  // Float add (loses precision!)

// RIGHT: int32 + int32 in integer domain
a.vpaddd(ymm0, ymm0, ymm1);  // Integer add (perfect!)

// If you need float output:
a.vpaddd(ymm0, ymm0, ymm1);  // Integer add first
a.vcvtdq2ps(ymm0, ymm0);      // Convert result to float (1 conversion)
```

**Key conversion instructions**:
- `vcvtdq2ps`: int32 → float32
- `vcvttps2dq`: float32 → int32 (truncate)
- `vcvtps2dq`: float32 → int32 (round)

---

### 05: Loop Generation
**File**: `asmjit_05_loops.cpp`

**What it teaches**:
- Creating labels for jump targets
- Conditional jumps (`jl`, `jz`)
- Loop counters and bounds checking
- Generating vectorized loops

**Key patterns**:
```cpp
// Create labels
asmjit::Label loop_start = a.new_label();
asmjit::Label loop_end = a.new_label();

// Bind label to current position
a.bind(loop_start);

// ... loop body ...

// Conditional jump
a.cmp(r8, rcx);        // Compare counter with bound
a.jl(loop_start);      // Jump if less (loop continue)
```

**Memory addressing**:
```cpp
ptr(base_reg, index_reg, shift)  // [base + index << shift]

a.vmovdqu(ymm0, ptr(rdi, r8, 2));
// Generates: vmovdqu ymm0, [rdi + r8*4]
// shift=2 means *4 (for 4-byte int32)
```

---

## The Critical Problem These Examples Solve

### Problem: Polymorphic Branching in Float Domain

If you use branching dispatch that always operates in the float domain:

```cpp
// WRONG approach
for (size_t i = 0; i < n; i += 8) {
  __m256 acc = _mm256_setzero_ps();  // Always float!

  if (type_a == Int32) {
    acc = _mm256_cvtepi32_ps(load_int32(a, i));  // Convert!
  }
  // ...operate in float domain...

  if (output_type == Int32) {
    store_int32(out, _mm256_cvttps_epi32(acc));  // Convert back!
  }
}
```

**Problems**:
1. ❌ Loses precision (float32 has only 24-bit mantissa, int32 is 32-bit)
2. ❌ Wasteful conversions (2+ conversions per operation)
3. ❌ Wrong instructions (uses `vaddps` instead of `vpaddd`)
4. ❌ 10-30% slower

### Solution: Compute-Type-Aware Code Generation

With JIT, you generate the right compute path:

```cpp
// For int32 + int32 -> int32
a.vpaddd(ymm0, ymm0, ymm1);  // Pure integer, no conversion!

// For int32 + float32 -> float32
a.vcvtdq2ps(ymm0, ymm0);     // Convert int32 input
a.vaddps(ymm0, ymm0, ymm1);  // Float add
```

## Next Steps

After understanding these examples, you're ready to:

1. Design a compute-type inference system
2. Build a kernel generator that emits the right instructions
3. Handle multiple input types and output types correctly
4. Benchmark against polymorphic branching (expect 10-30% speedup + correctness)

## Running All Examples

```bash
# Build all examples
bazel build -c opt //examples:asmjit_01_basic_function \
                   //examples:asmjit_02_integer_simd \
                   //examples:asmjit_03_float_simd \
                   //examples:asmjit_04_type_conversion \
                   //examples:asmjit_05_loops

# Run them in order
for i in 01 02 03 04 05; do
  echo "Running example $i..."
  bazel run -c opt //examples:asmjit_${i}_*
  echo
done
```
