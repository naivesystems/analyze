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

"""Starlark rules for building protocol buffers."""

load("//proto/private:native.bzl", "NativeProtoInfo", "native_proto_common")
load("//proto/private/rules:proto_descriptor_set.bzl", _proto_descriptor_set = "proto_descriptor_set")

_MIGRATION_TAG = "__PROTO_RULES_MIGRATION_DO_NOT_USE_WILL_BREAK__"

def _add_migration_tag(attrs):
    if "tags" in attrs and attrs["tags"] != None:
        attrs["tags"] = attrs["tags"] + [_MIGRATION_TAG]
    else:
        attrs["tags"] = [_MIGRATION_TAG]
    return attrs

def proto_lang_toolchain(**attrs):
    """Bazel proto_lang_toolchain rule.

    https://docs.bazel.build/versions/master/be/protocol-buffer.html#proto_lang_toolchain

    Args:
      **attrs: Rule attributes
    """

    # buildifier: disable=native-proto
    native.proto_lang_toolchain(**_add_migration_tag(attrs))

def proto_library(**attrs):
    """Bazel proto_library rule.

    https://docs.bazel.build/versions/master/be/protocol-buffer.html#proto_library

    Args:
      **attrs: Rule attributes
    """

    # buildifier: disable=native-proto
    native.proto_library(**_add_migration_tag(attrs))

proto_descriptor_set = _proto_descriptor_set

# Encapsulates information provided by `proto_library`.
#
# https://docs.bazel.build/versions/master/skylark/lib/ProtoInfo.html
ProtoInfo = NativeProtoInfo

# Utilities for protocol buffers.
#
# https://docs.bazel.build/versions/master/skylark/lib/proto_common.html
proto_common = native_proto_common
