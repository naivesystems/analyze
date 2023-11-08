load("//:z3.bzl", "gen_api_hdr", "gen_gparams_register_modules", "gen_hpp_from_pyg", "gen_mem_initializer_cpp", "gen_pat_db", "gen_tactic", "gen_tactic_runfile")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],
)

gen_hpp_from_pyg(
    name = "all_hpp_files",
    srcs = glob([
        "**/*.pyg",
    ]),
)

gen_api_hdr(
    name = "api_updates_files",
    srcs = glob([
        "api/*.h",
    ]),
)

gen_pat_db(
    name = "database_pat_db_files",
    input_smt2 = "ast/pattern/database.smt2",
)

gen_tactic_runfile(
    name = "gen_tactic_runfile",
    tactic_hdrs = glob([
        "**/*.h",
    ]),
)

gen_tactic(
    name = "tactic_files",
    runfile = ":gen_tactic_runfile",
)

gen_gparams_register_modules(
    name = "gparams_register_modules",
    hdrs = glob([
        "**/*.h",
    ]),
)

gen_mem_initializer_cpp(
    name = "mem_initializer_cpp",
    hdrs = glob([
        "**/*.h",
    ]),
)

genrule(
    name = "z3_version_header",
    srcs = ["util/z3_version.h.in"],
    outs = ["util/z3_version.h"],
    cmd = '''sed -e 's/@Z3_VERSION_MAJOR@/4/g'  \
                -e 's/@Z3_VERSION_MINOR@/12/g'  \
                -e 's/@Z3_VERSION_PATCH@/1/g'   \
                -e 's/@Z3_VERSION_TWEAK@/0/g'   \
                -e 's/@Z3_FULL_VERSION@/"4.12.1.0"/g'   \
                $(SRCS) > $(OUTS)''',
)

cc_library(
    name = "z3-lib",
    srcs = glob(
        [
            "**/*.cpp",
            "**/*.h",
        ],
        exclude = [
            "api/julia/*.cpp",
            "api/julia/*.h",
            "shell/**",
            "test/**",
        ],
    ) + [
        ":api_updates_files",
        ":all_hpp_files",
        ":database_pat_db_files",
        ":tactic_files",
        ":gparams_register_modules",
        ":mem_initializer_cpp",
        ":z3_version_header",
    ],
    hdrs = glob([
        "api/*.h",
    ]),
    copts = [
        "-D_MP_INTERNAL",
        "-std=c++17",
    ],
    includes = ["."],
    strip_include_prefix = "api/",
)
