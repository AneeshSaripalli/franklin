cc_library(
    name = "core",
    hdrs = [
        "bf16.hpp",
        "compiler_macros.hpp",
        "data_type_enum.hpp",
        "error_collector.hpp",
        "matrix.hpp",
        "math_utils.hpp",
    ],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "interpreter",
    hdrs = ["interpreter.hpp"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        ":core",
        "//container:container",
    ],
)

cc_test(
    name = "bf16_test",
    size = "small",
    srcs = ["bf16_test.cpp"],
    copts = [
        "-std=c++20",
        "-march=native",
    ],
    deps = [
        ":core",
        "@googletest//:gtest_main",
    ],
)
