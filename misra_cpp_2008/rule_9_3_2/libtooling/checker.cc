/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
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
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
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

class NonConstHandlesCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
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
  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
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
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new NonConstHandlesCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_9_3_2
}  // namespace misra_cpp_2008
