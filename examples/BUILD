cc_binary(
    name = "interpreter_example",
    srcs = ["interpreter_example.cpp"],
    copts = [
        "-std=c++20",
        "-march=native",
    ],
    deps = [
        "//container",
        "//core:interpreter",
    ],
)

cc_binary(
    name = "asmjit_01_basic_function",
    srcs = ["asmjit_01_basic_function.cpp"],
    copts = [
        "-std=c++20",
        "-O3",
    ],
    deps = [
        "@asmjit//:asmjit",
    ],
)

cc_binary(
    name = "asmjit_02_integer_simd",
    srcs = ["asmjit_02_integer_simd.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-O3",
    ],
    deps = [
        "@asmjit//:asmjit",
    ],
)

cc_binary(
    name = "asmjit_03_float_simd",
    srcs = ["asmjit_03_float_simd.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-O3",
    ],
    deps = [
        "@asmjit//:asmjit",
    ],
)

cc_binary(
    name = "asmjit_04_type_conversion",
    srcs = ["asmjit_04_type_conversion.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-O3",
    ],
    deps = [
        "@asmjit//:asmjit",
    ],
)

cc_binary(
    name = "asmjit_05_loops",
    srcs = ["asmjit_05_loops.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-O3",
    ],
    deps = [
        "@asmjit//:asmjit",
    ],
)
