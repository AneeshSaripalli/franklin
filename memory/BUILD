cc_library(
    name = "buddy_allocator",
    srcs = ["buddy_allocator.cpp"],
    hdrs = [
        "buddy_allocator.hpp",
        "buddy_allocator_adapter.hpp",
    ],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
    deps = [
        "//container:container",
        "//core:core",
    ],
)

cc_test(
    name = "buddy_allocator_adapter_test",
    srcs = ["buddy_allocator_adapter_test.cpp"],
    copts = ["-std=c++20"],
    deps = [
        ":buddy_allocator",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
