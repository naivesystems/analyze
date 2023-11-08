# Copyright 2019 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""All development and production dependencies of rules_proto."""

dependencies = {
    "bazel_skylib": {
        "sha256": "97e70364e9249702246c0e9444bccdc4b847bed1eb03c5a3ece4f83dfe6abc44",
        "urls": [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.0.2/bazel-skylib-1.0.2.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.0.2/bazel-skylib-1.0.2.tar.gz",
        ],
    },
    "com_github_protocolbuffers_protobuf": {
        "sha256": "9b4ee22c250fe31b16f1a24d61467e40780a3fbb9b91c3b65be2a376ed913a1a",
        "strip_prefix": "protobuf-3.13.0",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/archive/v3.13.0.tar.gz",
            "https://github.com/protocolbuffers/protobuf/archive/v3.13.0.tar.gz",
        ],
    },
    "com_google_protobuf_protoc_linux_aarch64": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "5f6f59be05ce91425195dc689f5faa59284efb4799526b6f92a7a91efe5702fd",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-aarch_64.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-aarch_64.zip",
        ],
    },
    "com_google_protobuf_protoc_linux_ppc": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "b5ff852d27f81a88b202e0d833a5f8c2f72ae23e0d36d4b3a2750a01b723ecf6",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-ppcle_64.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-ppcle_64.zip",
        ],
    },
    "com_google_protobuf_protoc_linux_s390x": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "11f95a50e193eb3d423240bc84a0a0f94b6e2043e819ba0bebcf24c7feaa3cde",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-s390x.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-s390x.zip",
        ],
    },
    "com_google_protobuf_protoc_linux_x86_32": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "9b91545dd085b148c54ec6e701b0d4dab1f9db95bc6574b2d1c27309750bc3b9",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-x86_32.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-x86_32.zip",
        ],
    },
    "com_google_protobuf_protoc_linux_x86_64": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "4a3b26d1ebb9c1d23e933694a6669295f6a39ddc64c3db2adf671f0a6026f82e",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-x86_64.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-linux-x86_64.zip",
        ],
    },
    "com_google_protobuf_protoc_macos_x86_64": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "a201954cc7d1a309b5f4feacd23a0abcf3ffc20eb15e79c9a0856a5804f6c34c",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-osx-x86_64.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-osx-x86_64.zip",
        ],
    },
    "com_google_protobuf_protoc_windows_x86_32": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "da7ac5d046810ee44c13bd92c6bc034763d483b506e697baf278e2769730716c",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-win32.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-win32.zip",
        ],
    },
    "com_google_protobuf_protoc_windows_x86_64": {
        "build_file": "@rules_proto//proto/private:BUILD.protoc",
        "sha256": "326a18c917cce8bc58fa6741260f6fb733186ffdab728a952b4cf31e57a76b91",
        "urls": [
            "https://mirror.bazel.build/github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-win64.zip",
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protoc-3.13.0-win64.zip",
        ],
    },
    # Dependency of `com_github_protocolbuffers_protobuf`.
    "rules_cc": {
        "sha256": "29daf0159f0cf552fcff60b49d8bcd4f08f08506d2da6e41b07058ec50cfeaec",
        "strip_prefix": "rules_cc-b7fe9697c0c76ab2fd431a891dbb9a6a32ed7c3e",
        "urls": [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_cc/archive/b7fe9697c0c76ab2fd431a891dbb9a6a32ed7c3e.tar.gz",
            "https://github.com/bazelbuild/rules_cc/archive/b7fe9697c0c76ab2fd431a891dbb9a6a32ed7c3e.tar.gz",
        ],
    },
    # Dependency of `com_github_protocolbuffers_protobuf`.
    "rules_java": {
        "sha256": "f5a3e477e579231fca27bf202bb0e8fbe4fc6339d63b38ccb87c2760b533d1c3",
        "strip_prefix": "rules_java-981f06c3d2bd10225e85209904090eb7b5fb26bd",
        "urls": [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_java/archive/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz",
            "https://github.com/bazelbuild/rules_java/archive/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz",
        ],
    },
    # Dependency of `com_github_protocolbuffers_protobuf`.
    "rules_python": {
        "sha256": "e5470e92a18aa51830db99a4d9c492cc613761d5bdb7131c04bd92b9834380f6",
        "strip_prefix": "rules_python-4b84ad270387a7c439ebdccfd530e2339601ef27",
        "urls": [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_python/archive/4b84ad270387a7c439ebdccfd530e2339601ef27.tar.gz",
            "https://github.com/bazelbuild/rules_python/archive/4b84ad270387a7c439ebdccfd530e2339601ef27.tar.gz",
        ],
    },
    # Dependency of `com_github_protocolbuffers_protobuf`.
    "six": {
        "build_file": "@com_github_protocolbuffers_protobuf//:third_party/six.BUILD",
        "sha256": "d16a0141ec1a18405cd4ce8b4613101da75da0e9a7aec5bdd4fa804d0e0eba73",
        "urls": [
            "https://mirror.bazel.build/pypi.python.org/packages/source/s/six/six-1.12.0.tar.gz",
            "https://pypi.python.org/packages/source/s/six/six-1.12.0.tar.gz",
        ],
    },
    # Dependency of `com_github_protocolbuffers_protobuf`.
    "zlib": {
        "build_file": "@com_github_protocolbuffers_protobuf//:third_party/zlib.BUILD",
        "sha256": "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
        "strip_prefix": "zlib-1.2.11",
        "urls": [
            "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
            "https://zlib.net/zlib-1.2.11.tar.gz",
        ],
    },
}

maven_dependencies = {
    "com_google_protobuf_protobuf_java": {
        "jar_sha256": "97d5b2758408690c0dc276238707492a0b6a4d71206311b6c442cdc26c5973ff",
        "jar_urls": [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/protobuf/protobuf-java/3.13.0/protobuf-java-3.13.0.jar",
            "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java/3.13.0/protobuf-java-3.13.0.jar",
        ],
        "licenses": ["notice"],
        "srcjar_sha256": "ced0309df90244c977361fa14ebc848ff1fef4910a051b2ee686a18b3840710c",
        "srcjar_urls": [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/protobuf/protobuf-java/3.13.0/protobuf-java-3.13.0-sources.jar",
            "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java/3.13.0/protobuf-java-3.13.0-sources.jar",
        ],
    },
    "com_google_protobuf_protobuf_java_util": {
        "jar_sha256": "d9de66b8c9445905dfa7064f6d5213d47ce88a20d34e21d83c4a94a229e14e62",
        "jar_urls": [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/protobuf/protobuf-java-util/3.13.0/protobuf-java-util-3.13.0.jar",
            "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java-util/3.13.0/protobuf-java-util-3.13.0.jar",
        ],
        "licenses": ["notice"],
        "srcjar_sha256": "6ebe9b3fb6ab4f5aebf7bf72fc0b7fb967b76f6d94bdc347b7fcdf32ef25dd97",
        "srcjar_urls": [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/protobuf/protobuf-java-util/3.13.0/protobuf-java-util-3.13.0-sources.jar",
            "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java-util/3.13.0/protobuf-java-util-3.13.0-sources.jar",
        ],
    },
    "com_google_protobuf_protobuf_javalite": {
        "jar_sha256": "3b018e3f82cd38de3b7001edb9427b470ebc20ab1cec84ef94e41e6fa4bf79a3",
        "jar_urls": [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/protobuf/protobuf-javalite/3.13.0/protobuf-javalite-3.13.0.jar",
            "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-javalite/3.13.0/protobuf-javalite-3.13.0.jar",
        ],
        "licenses": ["notice"],
        "srcjar_sha256": "413fdf5068f28a402c1ea5de3b98cb8105b656e690c52ac3c4e817831ca6bace",
        "srcjar_urls": [
            "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/protobuf/protobuf-javalite/3.13.0/protobuf-javalite-3.13.0-sources.jar",
            "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-javalite/3.13.0/protobuf-javalite-3.13.0-sources.jar",
        ],
    },
}

def _protobuf_workspace_impl(ctx):
    ctx.symlink(ctx.attr._build, "BUILD")
    ctx.symlink(ctx.attr._protobuf_bzl, "protobuf.bzl")
    ctx.symlink(ctx.attr._protobuf_deps_bzl, "protobuf_deps.bzl")

protobuf_workspace = repository_rule(
    implementation = _protobuf_workspace_impl,
    attrs = {
        "_build": attr.label(default = "@rules_proto//proto/private:BUILD.release"),
        "_protobuf_bzl": attr.label(default = "@com_github_protocolbuffers_protobuf//:protobuf.bzl"),
        "_protobuf_deps_bzl": attr.label(default = "@com_github_protocolbuffers_protobuf//:protobuf_deps.bzl"),
    },
)

def protoc_binary(name, srcs, executable, **kwargs):
    """protoc_binary makes a copy of the protoc binary to bazel-bin.

This is a workaround to make sure protoc can be used with attributes
which don't allow files."""

    # TODO(yannic) replace this with a symlink after https://github.com/bazelbuild/bazel/pull/10695
    native.genrule(
        name = name,
        executable = True,
        srcs = srcs,
        outs = [executable],
        cmd_bash = "cp $< $@ && chmod +x $@",
        cmd_bat = "copy $< $@",
        **kwargs
    )
