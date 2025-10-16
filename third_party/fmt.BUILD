cc_library(
    name = "fmt",
    srcs = glob([
        "src/*.cc",
    ], exclude = [
        "src/fmt.cc",
    ]),
    hdrs = glob([
        "include/fmt/*.h",
    ]),
    includes = ["include"],
    copts = ["-std=c++20"],
    visibility = ["//visibility:public"],
)
