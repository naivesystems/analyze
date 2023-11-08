// Copyright 2020 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include "gtest/gtest.h"
#include "tests/utils/workspace_constants.h"
#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;
using google::protobuf::FileDescriptorProto;
using google::protobuf::FileDescriptorSet;

namespace rulesproto {
namespace {

std::string GetRlocation(const std::string& file) {
  static std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest());
  return runfiles->Rlocation(file);
}

template <typename T, typename K>
bool Contains(const T& container, const K& key) {
  return container.find(key) != container.end();
}

std::vector<std::string> ReadFileDescriptorSet(const std::string& path) {
  std::ifstream input(path, std::ifstream::binary);
  EXPECT_TRUE(input) << "Could not open " << path;

  FileDescriptorSet file_descriptor_set;
  EXPECT_TRUE(file_descriptor_set.ParseFromIstream(&input));

  std::set<std::string> unordered_proto_files;
  for (FileDescriptorProto file_descriptor : file_descriptor_set.file()) {
    EXPECT_FALSE(Contains(unordered_proto_files, file_descriptor.name()))
        << "Already saw " << file_descriptor.name();
    unordered_proto_files.insert(file_descriptor.name());
  }

  std::vector<std::string> proto_files(
      unordered_proto_files.begin(), unordered_proto_files.end());
  std::sort(proto_files.begin(), proto_files.end());
  return proto_files;
}

void AssertFileDescriptorSetContains(
    const std::string& path,
    const std::vector<std::string>& expected_proto_files) {
  std::vector<std::string> actual_proto_files =
      ReadFileDescriptorSet(
          GetRlocation(rulesproto::kWorkspaceRlocation + path));
  EXPECT_EQ(expected_proto_files, actual_proto_files);
}

}  // namespace

TEST(ProtoDescriptorSetTest, NoProtos) {
  AssertFileDescriptorSetContains(
      "tests/rules/proto_descriptor_set/no_protos.pb", {});
}

TEST(ProtoDescriptorSetTest, WellKnownProtos) {
  AssertFileDescriptorSetContains(
      "tests/rules/proto_descriptor_set/well_known_protos.pb",
      {
          "google/protobuf/any.proto",
          "google/protobuf/api.proto",
          "google/protobuf/compiler/plugin.proto",
          "google/protobuf/descriptor.proto",
          "google/protobuf/duration.proto",
          "google/protobuf/empty.proto",
          "google/protobuf/field_mask.proto",
          "google/protobuf/source_context.proto",
          "google/protobuf/struct.proto",
          "google/protobuf/timestamp.proto",
          "google/protobuf/type.proto",
          "google/protobuf/wrappers.proto",
      });
}

}  // namespace rulesproto
