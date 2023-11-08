load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

bzl_library(
    name = "defs",
    srcs = [
        "defs.bzl",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//proto/private:native",
        "//proto/private/rules:proto_descriptor_set",
    ],
)
