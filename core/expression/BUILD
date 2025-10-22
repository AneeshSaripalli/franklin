cc_library(
    name = "ast",
    srcs = ["ast.cpp"],
    hdrs = ["ast.hpp"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "type_inference",
    hdrs = ["type_inference.hpp"],
    visibility = ["//visibility:public"],
    deps = [":ast"],
)

cc_library(
    name = "parser",
    hdrs = ["parser.hpp"],
    visibility = ["//visibility:public"],
    deps = [":ast"],
)

cc_library(
    name = "optimizer",
    hdrs = ["optimizer.hpp"],
    visibility = ["//visibility:public"],
    deps = [":ast"],
)

cc_library(
    name = "fusion_analysis",
    hdrs = ["fusion_analysis.hpp"],
    visibility = ["//visibility:public"],
    deps = [":ast"],
)

cc_library(
    name = "expression",
    visibility = ["//visibility:public"],
    deps = [
        ":ast",
        ":type_inference",
        ":parser",
        ":optimizer",
        ":fusion_analysis",
    ],
)

cc_test(
    name = "ast_test",
    size = "small",
    srcs = ["ast_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":ast",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "type_inference_test",
    size = "small",
    srcs = ["type_inference_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":ast",
        ":type_inference",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "parser_test",
    size = "small",
    srcs = ["parser_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":ast",
        ":parser",
        ":type_inference",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "optimizer_test",
    size = "small",
    srcs = ["optimizer_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":ast",
        ":optimizer",
        ":parser",
        ":type_inference",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "fusion_analysis_test",
    size = "small",
    srcs = ["fusion_analysis_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":ast",
        ":fusion_analysis",
        ":parser",
        ":type_inference",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
