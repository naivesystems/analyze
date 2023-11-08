workspace(name = "naivesystems_analyzer")

# The following lines related to LLVM are migrated from
# third_party/llvm-project/utils/bazel/examples/submodule/WORKSPACE

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

new_local_repository(
    name = "bazel_skylib",
    build_file = "third_party/github.com/bazelbuild/bazel-skylib/BUILD",
    path = "third_party/github.com/bazelbuild/bazel-skylib",
)

new_local_repository(
    name = "zlib",
    build_file = "third_party/github.com/protocolbuffers/protobuf/third_party/zlib.BUILD",
    path = "third_party/github.com/madler/zlib",
)

new_local_repository(
    name = "z3",
    build_file = "third_party/z3/BUILD",
    path = "third_party/z3",
)

local_repository(
    name = "rules_cc",
    path = "third_party/github.com/bazelbuild/rules_cc",
)

local_repository(
    name = "rules_java",
    path = "third_party/github.com/bazelbuild/rules_java",
)

local_repository(
    name = "rules_proto",
    path = "third_party/github.com/bazelbuild/rules_proto",
)

new_local_repository(
    name = "rules_python",
    build_file = "third_party/github.com/bazelbuild/rules_python/BUILD",
    path = "third_party/github.com/bazelbuild/rules_python",
)

local_repository(
    name = "rules_jvm_external",
    path = "third_party/github.com/bazelbuild/rules_jvm_external",
)

new_local_repository(
    name = "llvm-raw",
    build_file_content = "# empty",
    path = "third_party/llvm-project",
)

load("@llvm-raw//utils/bazel:configure.bzl", "llvm_configure", "llvm_disable_optional_support_deps")

llvm_configure(name = "llvm-project")

# Disables optional dependencies for Support like zlib and terminfo. You may
# instead want to configure them using the macros in the corresponding bzl
# files.
llvm_disable_optional_support_deps()

local_repository(
    name = "com_github_gflags_gflags",
    path = "third_party/github.com/gflags/gflags",
)

local_repository(
    name = "com_github_google_benchmark",
    path = "third_party/github.com/google/benchmark",
)

local_repository(
    name = "com_github_google_glog",
    path = "third_party/github.com/google/glog",
)

local_repository(
    name = "com_google_absl",
    path = "third_party/github.com/abseil/abseil-cpp",
)

local_repository(
    name = "com_google_googletest",
    path = "third_party/github.com/google/googletest",
)

local_repository(
    name = "com_google_protobuf",
    path = "third_party/github.com/protocolbuffers/protobuf",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()
