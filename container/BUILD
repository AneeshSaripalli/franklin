cc_library(
    name = "container",
    hdrs = [
        "column.hpp",
        "dynamic_bitset.hpp",
    ],
    copts = ["-std=c++20"],
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
    ],
    deps = [
        ":container",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
