cc_library(
    name = "checker",
    srcs = ["checker.cc"],
    hdrs = ["checker.h"],
    deps = [
        "//analyzer/proto:analyzer_cc_proto",
        "//misra:proto_util",
        "//misra/libtooling_utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@llvm-project//clang:tooling",
    ],
)

cc_library(
    name = "rule_10_3_1_lib",
    srcs = ["main.cc"],
    hdrs = ["lib.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker",
        "//libtooling_includes:cmd_options",
        "//misra:proto_util",
        "//misra/libtooling_utils",
        "//podman_image/bigmain:suffix_rule",
        "@com_github_google_glog//:glog",
        "@llvm-project//clang:tooling",
    ],
    alwayslink = True,
)

cc_binary(
    name = "rule_10_3_1",
    srcs = ["realmain.cc"],
    deps = [
        ":rule_10_3_1_lib",
        "//libtooling_includes:cmd_options",
    ],
)
