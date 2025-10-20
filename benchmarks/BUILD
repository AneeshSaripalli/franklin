cc_test(
    name = "example_benchmark",
    srcs = ["example_benchmark.cpp"],
    copts = ["-std=c++20"],
    deps = ["@com_github_google_benchmark//:benchmark"],
    tags = ["benchmark", "small"],
)

cc_test(
    name = "dynamic_bitset_benchmark",
    srcs = ["dynamic_bitset_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-O3",
        "-march=native",
        "-DNDEBUG",
    ],
    deps = [
        "//container",
        "@com_github_google_benchmark//:benchmark",
    ],
    tags = ["benchmark", "performance"],
)


cc_test(
    name = "bitset_density_benchmark",
    srcs = ["bitset_density_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-O3",
        "-march=native",
        "-mavx2",
        "-DNDEBUG",
    ],
    deps = [
        "//container",
        "//memory",
        "@com_github_google_benchmark//:benchmark",
    ],
    tags = ["benchmark", "performance", "avx2", "density"],
)

cc_binary(
    name = "bitset_benchmark",
    srcs = ["bitset_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512vpopcntdq",
        "-O3",
        "-march=native",
    ],
    deps = [
        "//container:container",
        "@google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "column_benchmark",
    srcs = ["column_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512bf16",
        "-O3",
        "-march=native",
    ],
    deps = [
        "//container:container",
        "@google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "bf16_microbench",
    srcs = ["bf16_microbench.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512bf16",
        "-O3",
        "-march=native",
    ],
    deps = [
        "//container:container",
        "@google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "bitmask_microbench",
    srcs = ["bitmask_microbench.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512bf16",
        "-O3",
        "-march=native",
    ],
    deps = [
        "//container:container",
        "@google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "kernel_fusion_benchmark",
    srcs = ["kernel_fusion_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mfma",
        "-O3",
        "-march=native",
    ],
    deps = [
        "//container:container",
        "@google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "asmjit_kernel_benchmark",
    srcs = ["asmjit_kernel_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-O3",
        "-march=native",
    ],
    deps = [
        "@asmjit//:asmjit",
    ],
)

cc_binary(
    name = "column_reduction_benchmark",
    srcs = ["column_reduction_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512vpopcntdq",
        "-mavx512bf16",
        "-O3",
        "-march=native",
    ],
    deps = [
        "//container:container",
        "@google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "cache_throughput_benchmark",
    srcs = ["cache_throughput_benchmark.cpp"],
    copts = [
        "-std=c++20",
        "-O3",
        "-march=native",
    ],
    deps = [
        "@google_benchmark//:benchmark",
    ],
)
