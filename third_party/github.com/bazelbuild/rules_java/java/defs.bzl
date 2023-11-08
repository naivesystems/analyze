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
"""Starlark rules for building Java projects."""

_MIGRATION_TAG = "__JAVA_RULES_MIGRATION_DO_NOT_USE_WILL_BREAK__"

def _add_tags(attrs):
    if "tags" in attrs and attrs["tags"] != None:
        attrs["tags"] = attrs["tags"] + [_MIGRATION_TAG]
    else:
        attrs["tags"] = [_MIGRATION_TAG]
    return attrs

def java_binary(**attrs):
    """Bazel java_binary rule.

    https://docs.bazel.build/versions/master/be/java.html#java_binary

    Args:
      **attrs: Rule attributes
    """
    native.java_binary(**_add_tags(attrs))

def java_import(**attrs):
    """Bazel java_import rule.

    https://docs.bazel.build/versions/master/be/java.html#java_import

    Args:
      **attrs: Rule attributes
    """
    native.java_import(**_add_tags(attrs))

def java_library(**attrs):
    """Bazel java_library rule.

    https://docs.bazel.build/versions/master/be/java.html#java_library

    Args:
      **attrs: Rule attributes
    """
    native.java_library(**_add_tags(attrs))

def java_lite_proto_library(**attrs):
    """Bazel java_lite_proto_library rule.

    https://docs.bazel.build/versions/master/be/java.html#java_lite_proto_library

    Args:
      **attrs: Rule attributes
    """
    native.java_lite_proto_library(**_add_tags(attrs))

def java_proto_library(**attrs):
    """Bazel java_proto_library rule.

    https://docs.bazel.build/versions/master/be/java.html#java_proto_library

    Args:
      **attrs: Rule attributes
    """
    native.java_proto_library(**_add_tags(attrs))

def java_test(**attrs):
    """Bazel java_test rule.

    https://docs.bazel.build/versions/master/be/java.html#java_test

    Args:
      **attrs: Rule attributes
    """
    native.java_test(**_add_tags(attrs))

def java_package_configuration(**attrs):
    """Bazel java_package_configuration rule.

    https://docs.bazel.build/versions/master/be/java.html#java_package_configuration

    Args:
      **attrs: Rule attributes
    """
    native.java_package_configuration(**_add_tags(attrs))

def java_plugin(**attrs):
    """Bazel java_plugin rule.

    https://docs.bazel.build/versions/master/be/java.html#java_plugin

    Args:
      **attrs: Rule attributes
    """
    native.java_plugin(**_add_tags(attrs))

def java_runtime(**attrs):
    """Bazel java_runtime rule.

    https://docs.bazel.build/versions/master/be/java.html#java_runtime

    Args:
      **attrs: Rule attributes
    """
    native.java_runtime(**_add_tags(attrs))

def java_toolchain(**attrs):
    """Bazel java_toolchain rule.

    https://docs.bazel.build/versions/master/be/java.html#java_toolchain

    Args:
      **attrs: Rule attributes
    """
    native.java_toolchain(**_add_tags(attrs))
