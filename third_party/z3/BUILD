package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],
)

py_library(
    name = "scripts",
    srcs = ["scripts/mk_genfile_common.py"],
    imports = ["scripts/"],
)

py_binary(
    name = "pyg2hpp",
    srcs = ["scripts/pyg2hpp.py"],
    deps = [":scripts"],
)

py_binary(
    name = "update_api",
    srcs = ["scripts/update_api.py"],
    deps = [":scripts"],
)

py_binary(
    name = "mk_pat_db",
    srcs = ["scripts/mk_pat_db.py"],
    deps = [":scripts"],
)

py_binary(
    name = "mk_install_tactic_cpp",
    srcs = ["scripts/mk_install_tactic_cpp.py"],
    deps = [":scripts"],
)

py_binary(
    name = "mk_gparams_register_modules_cpp",
    srcs = ["scripts/mk_gparams_register_modules_cpp.py"],
    deps = [":scripts"],
)

py_binary(
    name = "mk_mem_initializer_cpp",
    srcs = ["scripts/mk_mem_initializer_cpp.py"],
    deps = [":scripts"],
)

cc_library(
    name = "z3",
    deps = ["//src:z3-lib"],
)
