cc_library(
    name = "core",
    hdrs = [
        "bf16.hpp",
        "compiler_macros.hpp",
        "error_collector.hpp",
        "matrix.hpp",
    ],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "bf16_test",
    size = "small",
    srcs = ["bf16_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":core",
        "@googletest//:gtest_main",
    ],
)
