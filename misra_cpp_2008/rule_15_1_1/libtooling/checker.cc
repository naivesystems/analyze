/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/
#include "misra_cpp_2008/rule_15_1_1/libtooling/checker.hh"

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace misra_cpp_2008 {
namespace rule_15_1_1 {
namespace libtooling {

void ReportError(const Expr* e, SourceManager* sm, ResultsList* rs) {
  string error_message = "throw 语句的赋值表达式本身不应导致抛出异常";
  string path = misra::libtooling_utils::GetFilename(e, sm);
  int line = misra::libtooling_utils::GetLine(e, sm);
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(rs, path, line, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_15_1_1);
}

class ThrowVisitor : public RecursiveASTVisitor<ThrowVisitor> {
  const Expr* checking_expr_;
  SourceManager* source_manager_;
  ResultsList* results_list_;

 public:
  void Init(ResultsList* rs) { results_list_ = rs; }
  void Set(const Expr* e, SourceManager* sm) {
    checking_expr_ = e;
    source_manager_ = sm;
  }
  bool VisitStmt(Stmt* s) {
    if (llvm::dyn_cast<CXXThrowExpr>(s)) {
      ReportError(checking_expr_, source_manager_, results_list_);
      return false;
    }
    return true;
  }
};

bool MustHasException(const ExceptionSpecificationType exception_spec) {
  return exception_spec == clang::EST_Dynamic ||
         exception_spec == clang::EST_DependentNoexcept ||
         exception_spec == clang::EST_NoexceptFalse;
}

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    visitor_ = new ThrowVisitor();
    visitor_->Init(results_list);
    // Match variable declaration, only referenced in compound stmts
    finder->addMatcher(cxxThrowExpr(hasDescendant(callExpr().bind("expr"))),
                       this);
    finder->addMatcher(
        cxxThrowExpr(hasDescendant(cxxConstructExpr().bind("expr"))), this);
    finder->addMatcher(
        cxxThrowExpr(hasDescendant(cxxTemporaryObjectExpr().bind("expr"))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const Expr* e = result.Nodes.getNodeAs<Expr>("expr");
    if (misra::libtooling_utils::IsInSystemHeader(e, result.Context)) {
      return;
    }
    visitor_->Set(e, result.SourceManager);
    if (HasException(e)) {
      ReportError(e, result.SourceManager, results_list_);
    }
  }

 private:
  ResultsList* results_list_;
  ThrowVisitor* visitor_;
  bool HasException(const Expr* e);
};

bool Callback::HasException(const Expr* e) {
  if (auto ce = llvm::dyn_cast<CXXConstructExpr>(e)) {
    auto ctor = ce->getConstructor();
    if (MustHasException(ctor->getType()
                             ->getAs<FunctionProtoType>()
                             ->getExceptionSpecType()))
      return true;
    visitor_->TraverseStmt(ctor->getCanonicalDecl()->getBody());
  }
  if (auto ce = llvm::dyn_cast<CallExpr>(e)) {
    auto callee = ce->getCalleeDecl();
    if (MustHasException(callee->getFunctionType()
                             ->getAs<FunctionProtoType>()
                             ->getExceptionSpecType()))
      return true;
    visitor_->TraverseStmt(callee->getBody());
  }
  if (auto be = llvm::dyn_cast<BinaryOperator>(e)) {
    return HasException(be->getLHS()) || HasException(be->getRHS());
  }
  if (auto ue = llvm::dyn_cast<UnaryOperator>(e)) {
    return HasException(ue->getSubExpr());
  }
  if (auto te = llvm::dyn_cast<CXXThrowExpr>(e)) {
    return true;
  }
  // Optimistic that there are no exceptions
  return false;
}

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_15_1_1
}  // namespace misra_cpp_2008
