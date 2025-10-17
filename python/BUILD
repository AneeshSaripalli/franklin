load("@rules_cc//cc:defs.bzl", "cc_binary")

# Shared library for Python CFFI bindings
cc_binary(
    name = "libfranklin_c_api.so",
    srcs = [
        "franklin_c_api.cpp",
        "franklin_c_api.h",
    ],
    copts = [
        "-std=c++20",
        "-fPIC",
        "-march=native",
        "-O3",
    ],
    linkopts = [
        "-Wl,-soname,libfranklin_c_api.so",
    ],
    linkshared = True,
    visibility = ["//visibility:public"],
    deps = [
        "//container",
        "//core:interpreter",
    ],
)

# Python example (optional - can run directly too)
sh_binary(
    name = "hello_world",
    srcs = ["run.sh"],
    data = [
        ":libfranklin_c_api.so",
        "franklin.py",
        "hello_world.py",
    ],
)

# Convenience script
filegroup(
    name = "python_files",
    srcs = [
        "franklin.py",
        "hello_world.py",
        "requirements.txt",
    ],
    visibility = ["//visibility:public"],
)
