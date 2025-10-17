cc_library(
    name = "container",
    hdrs = [
        "column.hpp",
        "dynamic_bitset.hpp",
    ],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512vpopcntdq",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//core",
        "//memory",
    ],
)

cc_test(
    name = "dynamic_bitset_test",
    srcs = ["dynamic_bitset_test.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512vpopcntdq",
    ],
    deps = [
        ":container",
        "//core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "dynamic_bitset_stress_test",
    srcs = ["dynamic_bitset_stress_test.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
    ],
    deps = [
        ":container",
        "//core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)


cc_test(
    name = "column_test",
    srcs = ["column_test.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512bf16",
    ],
    deps = [
        ":container",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
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
        ":container",
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
        ":container",
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
        ":container",
        "@google_benchmark//:benchmark",
    ],
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
        ":container",
        "@google_benchmark//:benchmark",
    ],
)
