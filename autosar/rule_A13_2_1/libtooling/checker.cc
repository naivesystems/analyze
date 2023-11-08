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

#include "autosar/rule_A13_2_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kReturnStmtString[] = "returnStmt";
constexpr char kAssignOperatorDeclString[] = "assignOperatorDecl";

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "An assignment operator shall return a reference to \"this\".";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A13_2_1 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      returnStmt(hasAncestor(cxxMethodDecl().bind(kAssignOperatorDeclString)))
          .bind(kReturnStmtString),
      this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const ReturnStmt* return_stmt =
      result.Nodes.getNodeAs<ReturnStmt>(kReturnStmtString);
  const CXXMethodDecl* cxx_method_decl =
      result.Nodes.getNodeAs<CXXMethodDecl>(kAssignOperatorDeclString);
  if (cxx_method_decl->isOverloadedOperator() &&
      cxx_method_decl->getOverloadedOperator() == OO_Equal) {
    const CXXRecordDecl* cxx_record_decl = cxx_method_decl->getParent();
    std::string path = misra::libtooling_utils::GetFilename(
        cxx_method_decl, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(cxx_method_decl, result.SourceManager);
    if (!cxx_method_decl->getReturnType()->isReferenceType()) {
      ReportError(path, line_number, results_list_);
      return;
    }
    const ReferenceType* reference_type =
        cast<ReferenceType>(cxx_method_decl->getReturnType().getTypePtr());
    if (cxx_record_decl->getTypeForDecl()->getCanonicalTypeInternal() !=
        reference_type->getPointeeType()) {
      ReportError(path, line_number, results_list_);
      return;
    }
    const Expr* return_expr = return_stmt->getRetValue();
    if (!isa<UnaryOperator>(return_expr)) {
      ReportError(path, line_number, results_list_);
      return;
    }
    const UnaryOperator* deref_operaotr = cast<UnaryOperator>(return_expr);
    if (deref_operaotr->getOpcode() != UO_Deref) {
      ReportError(path, line_number, results_list_);
      return;
    }
    const Expr* this_pointer_expr = deref_operaotr->getSubExpr();
    if (!isa<CXXThisExpr>(this_pointer_expr)) {
      ReportError(path, line_number, results_list_);
      return;
    }
  }
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A13_2_1
}  // namespace autosar
