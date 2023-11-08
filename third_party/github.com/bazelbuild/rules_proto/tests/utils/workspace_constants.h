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

// Contains WORKSPACE-related constants (e.g. the rlocation prefix).
//
// This file exists so that users that prefer to vendor all their dependencies
// can create their own (private) version of this file that defines values that
// make sense for their workspace instead of, e.g., relying on fragile Copybara
// transformations.

namespace rulesproto {

constexpr char kWorkspaceRlocation[] = "rules_proto/";

}  // namespace rulesproto
