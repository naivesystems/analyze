#!/usr/bin/env bash

# Copyright 2020 The Bazel Authors. All rights reserved.
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

set -e -o pipefail
VERSION=$1
if [ -z "$VERSION" ]; then
  echo "usage: $0 version"
  exit 1
fi

URLS=(
    "https://github.com/protocolbuffers/protobuf/archive/v${VERSION}.tar.gz"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-linux-aarch_64.zip"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-linux-ppcle_64.zip"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-linux-s390x.zip"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-linux-x86_32.zip"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-linux-x86_64.zip"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-osx-x86_64.zip"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-win32.zip"
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protoc-${VERSION}-win64.zip"
    "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java/${VERSION}/protobuf-java-${VERSION}.jar"
    "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java/${VERSION}/protobuf-java-${VERSION}-sources.jar"
    "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java-util/${VERSION}/protobuf-java-util-${VERSION}.jar"
    "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-java-util/${VERSION}/protobuf-java-util-${VERSION}-sources.jar"
    "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-javalite/${VERSION}/protobuf-javalite-${VERSION}.jar"
    "https://repo1.maven.org/maven2/com/google/protobuf/protobuf-javalite/${VERSION}/protobuf-javalite-${VERSION}-sources.jar"
)

for U in "${URLS[@]}"; do
  echo "Downloading '${U}'..."
  MU="https://mirror.bazel.build/${U#"https://"}"
  SUM=$(curl -L --fail -q "$U" | shasum -a 256 | cut -d' ' -f1)
  echo ""
  echo "        'sha256': '${SUM}',"
  echo "        'urls': ["
  echo "            '$MU',"
  echo "            '$U',"
  echo "        ],"
done

echo "******************** IMPORTANT ********************"
echo ""
echo "Please upload the following files to mirror.bazel.build:"
echo ""
for U in "${URLS[@]}"; do
  echo "  $U"
done
