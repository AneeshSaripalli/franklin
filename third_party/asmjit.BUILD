cc_library(
    name = "asmjit",
    srcs = glob([
        "src/asmjit/**/*.cpp",
    ]),
    hdrs = glob([
        "src/asmjit/**/*.h",
    ]),
    includes = ["src"],
    copts = [
        "-std=c++17",
        "-DASMJIT_STATIC",
    ],
    visibility = ["//visibility:public"],
)
