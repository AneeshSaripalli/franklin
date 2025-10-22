cc_library(
    name = "parser",
    srcs = ["parser.cpp"],
    hdrs = ["parser.hpp"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        "//core",
        "@fmt//:fmt",
    ],
)

cc_test(
    name = "parser_test",
    size = "small",
    srcs = ["parser_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":parser",
        "//core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
