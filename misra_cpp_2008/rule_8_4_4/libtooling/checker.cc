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

#include "misra_cpp_2008/rule_8_4_4/libtooling/checker.h"

#include <glog/logging.h>

#include <iostream>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void ReportError(std::string filename, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      absl::StrFormat("函数标识符要么用于调用函数，要么以 & 开头");
  analyzer::proto::Result* pb_result = AddResultToResultsList(
      results_list, filename, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_8_4_4);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_8_4_4 {
namespace libtooling {

class FuncIdentifierCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;

    // First Matcher only matches the function idetifier that doesn't contain
    // callExpr in its parent. There are two examples that contain callExpr in
    // its parent:
    // 1. The immediate parent is callExpr.
    // 2. There is an ImplicitCast between callExpr and declRefExpr.
    // Each is excluded by using an unless.
    finder->addMatcher(
        declRefExpr(hasType(functionType()), unless(hasParent(callExpr())),
                    unless(hasParent(implicitCastExpr(hasParent(callExpr())))),
                    unless(hasParent(unaryOperator(hasOperatorName("&")))))
            .bind("func"),
        this);

    // Second Matcher matches the function idetifier that contain callExpr in
    // its parent. Then check whether the callExpr calls exactly this function
    // in run(). Report an Error if not. Compliant if the declRefExpr doesn't
    // have ImplicitCast as its parent (see good1.cc).
    finder->addMatcher(
        declRefExpr(hasType(functionType()),
                    hasParent(implicitCastExpr(
                        hasParent(callExpr(callee(decl().bind("func_call")))))),
                    unless(hasParent(unaryOperator(hasOperatorName("&")))))
            .bind("func"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    const clang::DeclRefExpr* func =
        result.Nodes.getNodeAs<clang::DeclRefExpr>("func");
    const clang::Decl* func_call =
        result.Nodes.getNodeAs<clang::Decl>("func_call");

    SourceManager* source_manager = result.SourceManager;

    if (func_call) {
      if (func_call->getAsFunction()->getNameAsString() !=
          func->getNameInfo().getName().getAsString()) {
        ReportError(
            misra::libtooling_utils::GetFilename(func, result.SourceManager),
            misra::libtooling_utils::GetLine(func, result.SourceManager),
            results_list_);
      }
      return;
    }

    ReportError(
        misra::libtooling_utils::GetFilename(func, result.SourceManager),
        misra::libtooling_utils::GetLine(func, result.SourceManager),
        results_list_);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new FuncIdentifierCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_8_4_4
}  // namespace misra_cpp_2008
