/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2023  Naive Systems Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// This rule only focus on one use, so it's not a dead store problem

#include "misra_cpp_2008/rule_7_3_4/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list_) {
  std::string error_message = absl::StrFormat("不得使用using指令");
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list_, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_7_3_4 {
namespace libtooling {

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  finder_.addMatcher(usingDirectiveDecl().bind("using"), this);
}

void Checker::run(const MatchFinder::MatchResult& result) {
  const UsingDirectiveDecl* using_ =
      result.Nodes.getNodeAs<UsingDirectiveDecl>("using");
  std::string path_ =
      misra::libtooling_utils::GetFilename(using_, result.SourceManager);
  int line_number_ =
      misra::libtooling_utils::GetLine(using_, result.SourceManager);
  const SourceLocation loc = using_->getLocation();
  if (loc.isInvalid() ||
      result.Context->getSourceManager().isInSystemHeader(loc)) {
    return;
  }
  ReportError(path_, line_number_, results_list_);
}

}  // namespace libtooling
}  // namespace rule_7_3_4
}  // namespace misra_cpp_2008
