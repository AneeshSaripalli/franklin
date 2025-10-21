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
        "-mavx512dq",
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
        "-mavx512dq",
        "-mavx512bf16",
    ],
    deps = [
        ":container",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "dynamic_bitset_adversarial_test",
    srcs = ["dynamic_bitset_adversarial_test.cpp"],
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
    name = "column_adversarial_test",
    srcs = ["column_adversarial_test.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512bf16",
    ],
    deps = [
        ":container",
        "//core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "critical_failing_tests",
    srcs = ["critical_failing_tests.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512vpopcntdq",
        "-mavx512bf16",
    ],
    deps = [
        ":container",
        "//core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "column_test_non_power_of_2",
    srcs = ["column_test_non_power_of_2.cpp"],
    copts = [
        "-std=c++20",
        "-mavx2",
        "-mavx512f",
        "-mavx512vl",
        "-mavx512dq",
        "-mavx512bf16",
    ],
    deps = [
        ":container",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
