cc_library(
    name = "libtooling_utils",
    srcs = ["libtooling_utils.cc"],
    hdrs = ["libtooling_utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@llvm-project//clang:tooling",
    ],
)

cc_test(
    name = "libtooling_utils_test",
    srcs = ["libtooling_utils_test.cc"],
    deps = [
        ":libtooling_utils",
        "@com_google_googletest//:gtest",
    ],
)
