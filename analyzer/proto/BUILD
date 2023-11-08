load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "analyzer_proto",
    srcs = [
        ":analyzer.proto",
        ":results.proto",
        ":suppression.proto",
    ],
)

cc_proto_library(
    name = "analyzer_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":analyzer_proto"],
)
