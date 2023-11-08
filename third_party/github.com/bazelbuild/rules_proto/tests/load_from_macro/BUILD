load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

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

load("//proto:defs.bzl", "proto_library")
load(":tags.bzl", "TAGS")

proto_library(
    name = "foo",
    srcs = ["foo.proto"],
    tags = TAGS,
)

bzl_library(
    name = "tags",
    srcs = ["tags.bzl"],
)
