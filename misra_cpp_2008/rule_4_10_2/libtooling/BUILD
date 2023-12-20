cc_library(
    name = "rule_4_10_2_lib",
    srcs = ["checker.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//misra:proto_util",
        "//misra/libtooling_utils",
        "//sdk/checker",
    ],
    alwayslink = True,
)

cc_binary(
    name = "rule_4_10_2",
    deps = [
        ":rule_4_10_2_lib",
        "//podman_image/bigmain",
    ],
)
