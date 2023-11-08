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

#include "autosar/rule_A12_6_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kCXXConstructorDeclString[] = "cxxConstructorDecl";

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "All class data members that are initialized by the constructor shall be initialized using member initializers";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A12_6_1 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      cxxConstructorDecl(unless(isImplicit())).bind(kCXXConstructorDeclString),
      this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXConstructorDecl* cxx_constructor_decl =
      result.Nodes.getNodeAs<CXXConstructorDecl>(kCXXConstructorDeclString);
  const CXXRecordDecl* cxx_record_decl = cxx_constructor_decl->getParent();
  std::set<const FieldDecl*> members = {};
  if (cxx_constructor_decl->hasBody()) {
    const Stmt* body = cxx_constructor_decl->getBody();
    for (const Stmt* child : body->children()) {
      // According to the type of lhs, there are two cases should be handled.
      // When such cases are matched, it means that a field variable is assigned
      // in the constructor body, so put it to the members set.
      if (const BinaryOperator* binary_operator =
              dyn_cast<BinaryOperator>(child)) {
        // If the lhs is a builtin type variable, the expr is a binary operator
        // type.
        if (binary_operator->isAssignmentOp()) {
          if (const MemberExpr* lhs =
                  dyn_cast<MemberExpr>(binary_operator->getLHS())) {
            if (const FieldDecl* field_decl =
                    dyn_cast<FieldDecl>(lhs->getMemberDecl())) {
              members.insert(field_decl);
            }
          }
        }
      } else if (const CXXOperatorCallExpr* cxx_operator_call_expr =
                     dyn_cast<CXXOperatorCallExpr>(child)) {
        // If the lhs is a cxx class, it invokes the overridden operator=
        // method, so the expr is a cxx operator call expr. The lhs is the
        // second child of the operator expr.
        if (cxx_operator_call_expr->isAssignmentOp()) {
          Stmt::const_child_iterator member_it =
              ++cxx_operator_call_expr->child_begin();
          if (const MemberExpr* member = dyn_cast<MemberExpr>(*member_it)) {
            if (const FieldDecl* field_decl =
                    dyn_cast<FieldDecl>(member->getMemberDecl())) {
              members.insert(field_decl);
            }
          }
        }
      }
    }
  }
  for (const CXXCtorInitializer* init : cxx_constructor_decl->inits()) {
    if (init->isMemberInitializer() && init->isWritten()) {
      members.erase(init->getMember());
    }
  }
  for (const FieldDecl* member : members) {
    std::string path = misra::libtooling_utils::GetFilename(
        cxx_constructor_decl, result.SourceManager);
    int line_number = misra::libtooling_utils::GetLine(cxx_constructor_decl,
                                                       result.SourceManager);
    ReportError(path, line_number, results_list_);
  }
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A12_6_1
}  // namespace autosar
