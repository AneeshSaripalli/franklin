workspace(name = "franklin")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Hedron's Compile Commands Extractor for Bazel
# https://github.com/hedronvision/bazel-compile-commands-extractor
http_archive(
    name = "hedron_compile_commands",
    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/0e990032f3c5a866e72615cf67e5ce22186dcb97.tar.gz",
    strip_prefix = "bazel-compile-commands-extractor-0e990032f3c5a866e72615cf67e5ce22186dcb97",
)

load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")
hedron_compile_commands_setup()

# fmt library
http_archive(
    name = "fmt",
    url = "https://github.com/fmtlib/fmt/releases/download/11.0.2/fmt-11.0.2.zip",
    strip_prefix = "fmt-11.0.2",
    sha256 = "40fc58bebcf38c759e11a7bd8fdc163507d2423ef5058bba7f26280c5b9c5465",
    build_file = "@//third_party:fmt.BUILD",
)

# Google Benchmark
http_archive(
    name = "com_github_google_benchmark",
    url = "https://github.com/google/benchmark/archive/v1.9.1.tar.gz",
    strip_prefix = "benchmark-1.9.1",
    sha256 = "32131c08ee31eeff2c8968d7e874f3cb648034377dfc32a4c377fa8796d84981",
)
