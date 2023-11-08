# Protobuf Rules for [Bazel](https://bazel.build)

* Postsubmit [![Build status](https://badge.buildkite.com/26d40f574d6f6026928bc271780782e5f168fe7e3595ea6d79.svg?branch=master)](https://buildkite.com/bazel/rules-proto)
* Postsubmit + Current Bazel Incompatible Flags [![Build status](https://badge.buildkite.com/9c0cf88b7ca5814cf12f4ef2741306074c8e30ef7dabce1a1a.svg?branch=master)](https://buildkite.com/bazel/rules-proto-plus-bazelisk-migrate)

This repository contains Starlark implementation of Protobuf rules in Bazel.

For the list of Proto rules, see the Bazel
[documentation](https://docs.bazel.build/versions/master/be/overview.html).

## Getting Started

To get started with `rules_proto`, add the following to your `WORKSPACE` file:

```python
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_proto",
    sha256 = "602e7161d9195e50246177e7c55b2f39950a9cf7366f74ed5f22fd45750cd208",
    strip_prefix = "rules_proto-97d8af4dc474595af3900dd85cb3a29ad28cc313",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
        "https://github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
    ],
)
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
rules_proto_dependencies()
rules_proto_toolchains()
```

Then, in your `BUILD` files, import and use the rules:

```python
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    ...
)
```

If you're migrating from the native proto rules to `rules_proto`, you can use
the following [buildifier](https://github.com/bazelbuild/buildtools/blob/master/buildifier/README.md)
command to automate the changes to your `BUILD` and `.bzl` files:

```bash
buildifier --lint=fix --warnings=native-proto <path/to/BUILD>
```

## Contributing

Bazel and `rules_proto` are the work of many contributors.
We appreciate your help!

To contribute, please read the contribution guidelines:
[CONTRIBUTING.md](https://github.com/bazelbuild/rules_proto/blob/master/CONTRIBUTING.md).

Note that the `rules_proto` use the GitHub issue tracker for bug reports and
feature requests only.

For asking questions see:

* [rules_proto mailing list](https://groups.google.com/forum/#!forum/proto-bazel-discuss)
* Slack channel `#proto` on [slack.bazel.build](https://slack.bazel.build)
