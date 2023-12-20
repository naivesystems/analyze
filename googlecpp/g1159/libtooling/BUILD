cc_library(
    name = "g1159_lib",
    srcs = ["checker.cc"],
    visibility = ["//visibility:public"],
    deps = ["//sdk/checker"],
    alwayslink = True,
)

cc_binary(
    name = "g1159",
    deps = [
        ":g1159_lib",
        "//podman_image/bigmain",
    ],
)
