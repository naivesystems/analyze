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

#include "misra_cpp_2008/rule_9_3_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "成员函数不应将非常量句柄返回到类数据";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_9_3_2 {
namespace libtooling {

bool isReferenceType(QualType qType) { return qType->getAs<ReferenceType>(); }

bool isPointerType(QualType qType) { return qType->getAs<PointerType>(); }

AST_MATCHER(FunctionDecl, hasConstReturnType) {
  auto returnType = Node.getReturnType();
  if (isPointerType(returnType)) {
    return returnType->getAs<PointerType>()
        ->getPointeeType()
        .isConstQualified();
  } else {
    return returnType.isConstQualified();
  }
}

AST_MATCHER(FunctionDecl, hasPointerReturnType) {
  auto returnType = Node.getReturnType();
  return isPointerType(returnType);
}

AST_MATCHER(FunctionDecl, hasReferReturnType) {
  auto returnType = Node.getReturnType();
  return isReferenceType(returnType);
}

class NonConstHandlesCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxMethodDecl(allOf(
            unless(hasConstReturnType()),
            anyOf(
                allOf(hasPointerReturnType(),
                      hasDescendant(
                          returnStmt(has(unaryOperator(allOf(
                                         hasOperatorName("&"),
                                         has(memberExpr(has(cxxThisExpr())))))))
                              .bind("return_stmt"))),
                allOf(hasReferReturnType(),
                      hasDescendant(
                          returnStmt(has(memberExpr(has(cxxThisExpr()))))
                              .bind("return_stmt")))))),
        this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const ReturnStmt* return_stmt =
        result.Nodes.getNodeAs<ReturnStmt>("return_stmt");

    if (misra::libtooling_utils::IsInSystemHeader(return_stmt,
                                                  result.Context)) {
      return;
    }

    ReportError(
        misra::libtooling_utils::GetFilename(return_stmt, result.SourceManager),
        misra::libtooling_utils::GetLine(return_stmt, result.SourceManager),
        results_list_);

    return;
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new NonConstHandlesCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_9_3_2
}  // namespace misra_cpp_2008
