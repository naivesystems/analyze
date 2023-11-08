load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_library(
    name = "checker",
    srcs = ["checker.cc"],
    hdrs = ["checker.h"],
    deps = [
        "//analyzer/proto:analyzer_cc_proto",
        "//misra:proto_util",
        "//misra/libtooling_utils",
        "@com_github_google_glog//:glog",
        "@llvm-project//clang:tooling",
    ],
)

cc_library(
    name = "rule_18_5_lib",
    srcs = ["main.cc"],
    hdrs = ["lib.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker",
        "//misra:proto_util",
        "//misra/libtooling_utils",
        "//podman_image/bigmain:suffix_rule",
        "@com_github_google_glog//:glog",
        "@llvm-project//clang:tooling",
    ],
    alwayslink = True,
)

cc_binary(
    name = "rule_18_5",
    srcs = ["realmain.cc"],
    deps = [
        ":rule_18_5_lib",
        "//libtooling_includes:cmd_options",
    ],
)
