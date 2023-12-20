cc_library(
    name = "checker",
    srcs = [
        "ast_checker.cc",
        "decl_checker.cc",
        "define_decl_checker.cc",
        "define_expr_checker.cc",
        "expr_checker.cc",
    ],
    hdrs = [
        "ast_checker.h",
        "checker.h",
        "decl_checker.h",
        "define_ast_checker.h",
        "define_decl_checker.h",
        "define_expr_checker.h",
        "expr_checker.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//analyzer/proto:analyzer_cc_proto",
        "//misra:proto_util",
        "//misra/libtooling_utils",
        "//podman_image/bigmain:suffix_rule",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@llvm-project//clang:tooling",
    ],
)
