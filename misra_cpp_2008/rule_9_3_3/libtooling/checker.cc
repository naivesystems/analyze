/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_9_3_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "如果成员函数可以设为静态，则应设为静态，否则如果可设为 const，则应设为 const";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_9_3_3 {
namespace libtooling {

AST_MATCHER(Expr, isLValue) { return Node.isLValue(); }

AST_MATCHER(Expr, isMemberExpr) {
  switch (Node.getStmtClass()) {
    case Stmt::MemberExprClass:
    case Stmt::CXXDependentScopeMemberExprClass:
    case Stmt::UnresolvedMemberExprClass:
      return true;
    default:
      return false;
  }
}

class StaticOrConstMethodCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        cxxMethodDecl(
            has(compoundStmt(unless(hasDescendant(expr(
                isMemberExpr(), anyOf(hasDescendant(declRefExpr(isLValue())),
                                      has(cxxThisExpr()))))))))
            .bind("static_method"),
        this);
    finder->addMatcher(
        cxxMethodDecl(
            has(compoundStmt(allOf(
                hasDescendant(expr(isMemberExpr(), has(cxxThisExpr()))),
                unless(hasDescendant(expr(
                    isMemberExpr(),
                    anyOf(hasDescendant(declRefExpr(isLValue())),
                          allOf(has(cxxThisExpr()), isLValue(),
                                unless(hasParent(implicitCastExpr(
                                    hasCastKind(CK_LValueToRValue)))))))))))))
            .bind("const_method"),
        this);
  }
  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const CXXMethodDecl* static_method =
        result.Nodes.getNodeAs<CXXMethodDecl>("static_method");

    const CXXConstructorDecl* static_constructor =
        result.Nodes.getNodeAs<CXXConstructorDecl>("static_method");
    if (nullptr != static_constructor) {
      return;
    }
    const CXXDestructorDecl* static_destructor =
        result.Nodes.getNodeAs<CXXDestructorDecl>("static_method");
    if (nullptr != static_destructor) {
      return;
    }

    if (nullptr != static_method) {
      if (misra::libtooling_utils::IsInSystemHeader(static_method,
                                                    result.Context)) {
        return;
      }
      if (!static_method->isStatic()) {
        ReportError(misra::libtooling_utils::GetFilename(static_method,
                                                         result.SourceManager),
                    misra::libtooling_utils::GetLine(static_method,
                                                     result.SourceManager),
                    results_list_);
      }
      return;
    }

    const CXXMethodDecl* const_method =
        result.Nodes.getNodeAs<CXXMethodDecl>("const_method");

    const CXXConstructorDecl* const_constructor =
        result.Nodes.getNodeAs<CXXConstructorDecl>("const_method");
    if (nullptr != const_constructor) {
      return;
    }
    const CXXDestructorDecl* const_destructor =
        result.Nodes.getNodeAs<CXXDestructorDecl>("const_method");
    if (nullptr != const_destructor) {
      return;
    }

    if (nullptr != const_method) {
      if (misra::libtooling_utils::IsInSystemHeader(const_method,
                                                    result.Context)) {
        return;
      }
      if (!const_method->isConst()) {
        ReportError(misra::libtooling_utils::GetFilename(const_method,
                                                         result.SourceManager),
                    misra::libtooling_utils::GetLine(const_method,
                                                     result.SourceManager),
                    results_list_);
      }
      return;
    }

    return;
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new StaticOrConstMethodCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_9_3_3
}  // namespace misra_cpp_2008
