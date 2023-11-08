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

#include "autosar/rule_A8_4_12/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace misra::libtooling_utils;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const string& path, int line_number,
                 ResultsList* results_list) {
  string error_message =
      "A std::unique_ptr shall be passed to a function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that the function replaces the managed object.";
  AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A8_4_12 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        parmVarDecl(
            hasAncestor(functionDecl(isDefinition()).bind("fd")),
            hasType(references(namedDecl(matchesName("::std::unique_ptr"),
                                         isExpansionInSystemHeader()))),
            unless(isExpansionInSystemHeader()))
            .bind("pvd"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const ParmVarDecl* pvd = result.Nodes.getNodeAs<ParmVarDecl>("pvd");
    const FunctionDecl* fd = result.Nodes.getNodeAs<FunctionDecl>("fd");
    if (pvd) {
      const Type* type = pvd->getType().getTypePtr();
      if (!type->isReferenceType()) return;  // (1)
      ASTVisitor Visitor;
      Visitor.TraverseDecl(const_cast<FunctionDecl*>(fd));
      bool is_replaced = false, is_moved = false;
      for (const CXXMemberCallExpr* call : Visitor.getMemberCalls()) {
        const DeclRefExpr* replaced_arg = dyn_cast<DeclRefExpr>(
            call->getImplicitObjectArgument()->IgnoreImpCasts());
        if (replaced_arg && replaced_arg->getDecl() == pvd &&
            call->getMethodDecl()->getName() == "reset")
          is_replaced = true;
      }
      for (const CXXOperatorCallExpr* op : Visitor.getOperatorCalls()) {
        if (op->isAssignmentOp()) {
          const DeclRefExpr* replaced_arg =
              dyn_cast<DeclRefExpr>(op->getArg(0)->IgnoreImpCasts());
          const DeclRefExpr* copied_arg =
              dyn_cast<DeclRefExpr>(op->getArg(1)->IgnoreImpCasts());
          const CallExpr* ce_arg =
              dyn_cast<CallExpr>(op->getArg(1)->IgnoreImpCasts());
          if (copied_arg && replaced_arg &&
              replaced_arg->getDecl() == copied_arg->getDecl())
            continue;  // self-assignment
          if (replaced_arg && replaced_arg->getDecl() == pvd)
            is_replaced = true;
          if (ce_arg && ce_arg->getDirectCallee() &&
              ce_arg->getNumArgs() == 1) {
            const FunctionDecl* ce_arg_callee = ce_arg->getDirectCallee();
            const DeclRefExpr* moved_arg =
                dyn_cast<DeclRefExpr>(ce_arg->getArg(0)->IgnoreImpCasts());
            if (IsInSystemHeader(ce_arg_callee, result.Context) &&
                ce_arg_callee->getQualifiedNameAsString() == "std::move" &&
                moved_arg && moved_arg->getDecl() == pvd)
              is_moved = true;
          }
        }
      }
      for (const CXXConstructExpr* cce : Visitor.getConstructExprs()) {
        if (cce->getNumArgs() == 1) {
          const CallExpr* ce_arg =
              dyn_cast<CallExpr>(cce->getArg(0)->IgnoreImpCasts());
          if (ce_arg && ce_arg->getDirectCallee() &&
              ce_arg->getNumArgs() == 1) {
            const FunctionDecl* ce_arg_callee = ce_arg->getDirectCallee();
            const DeclRefExpr* moved_arg =
                dyn_cast<DeclRefExpr>(ce_arg->getArg(0)->IgnoreImpCasts());
            if (IsInSystemHeader(ce_arg_callee, result.Context) &&
                ce_arg_callee->getQualifiedNameAsString() == "std::move" &&
                moved_arg && moved_arg->getDecl() == pvd)
              is_moved = true;
          }
        }
      }
      if (type->isLValueReferenceType() && is_replaced) return;  // (2)
      if (type->isRValueReferenceType() && is_moved) return;     // Exception
      ReportError(GetFilename(pvd, result.SourceManager),
                  GetLine(pvd, result.SourceManager), results_list_);
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A8_4_12
}  // namespace autosar
