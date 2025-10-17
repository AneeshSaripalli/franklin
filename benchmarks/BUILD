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
