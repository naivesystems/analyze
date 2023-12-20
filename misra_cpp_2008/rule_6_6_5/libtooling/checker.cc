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

#include "misra_cpp_2008/rule_6_6_5/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-6.6.5] 函数结尾必须有且仅有一个退出点";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_6_6_5 {
namespace libtooling {

void CheckFuncReturnCallback::Init(ResultsList* results_list,
                                   MatchFinder* finder) {
  results_list_ = results_list;

  // This rule essentially aims to report on function exits in the middle of the
  // execution. For multiple returns on the root level of a function body, like:
  //  void func() {
  //    // some codes
  //    return;
  //    // some other codes
  //    return;
  //   }
  // we do not consider it as an bad case for the part after the first return is
  // dead code.
  // So in this rule, reporting only returns in the sub expressions of the body
  // concerned will cover all incompliant cases.

  auto isConcerned = anyOf(hasParent(functionDecl()),
                           hasParent(cxxTryStmt(hasParent(functionDecl()))),
                           hasParent(cxxCatchStmt(hasParent(
                               cxxTryStmt(hasParent(functionDecl()))))));

  auto compound_matcher = compoundStmt(
      isConcerned,
      forEach(stmt(forEachDescendant(returnStmt().bind("return")))));

  auto other_return_matcher =
      compoundStmt(isConcerned, has(stmt(hasDescendant(returnStmt()))),
                   forEach(returnStmt().bind("return")));

  // match all exits in the middle of the function
  finder->addMatcher(compound_matcher, this);
  // once the function is incompliant, report all other exits in the root level
  // (see badcase 1)
  finder->addMatcher(other_return_matcher, this);
}

void CheckFuncReturnCallback::run(const MatchFinder::MatchResult& result) {
  const ReturnStmt* return_ = result.Nodes.getNodeAs<ReturnStmt>("return");

  if (misra::libtooling_utils::IsInSystemHeader(return_, result.Context)) {
    return;
  }

  ReportError(
      misra::libtooling_utils::GetFilename(return_, result.SourceManager),
      misra::libtooling_utils::GetLine(return_, result.SourceManager),
      results_list_);
  return;
}

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckFuncReturnCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_6_6_5
}  // namespace misra_cpp_2008
