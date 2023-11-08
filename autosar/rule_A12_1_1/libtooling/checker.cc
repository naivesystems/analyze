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

#include "autosar/rule_A12_1_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kCXXConstructorDeclString[] = "cxxConstructorDeclString";

void FindVirtualBases(const CXXRecordDecl* cxx_record_decl,
                      std::list<const CXXRecordDecl*>& virtual_bases) {
  if (cxx_record_decl) {
    for (const CXXBaseSpecifier& base : cxx_record_decl->bases()) {
      const CXXRecordDecl* base_cxx_record_decl =
          base.getType()->getAsCXXRecordDecl();
      if (base_cxx_record_decl && base.isVirtual()) {
        virtual_bases.emplace_back(base_cxx_record_decl);
      }
      FindVirtualBases(base_cxx_record_decl, virtual_bases);
    }
  }
}

// This function is used to find all virtual base class recursively.
std::list<const CXXRecordDecl*> FindVirtualBases(
    const CXXRecordDecl* cxx_record_decl) {
  std::list<const CXXRecordDecl*> virtual_bases = {};
  FindVirtualBases(cxx_record_decl, virtual_bases);
  return virtual_bases;
}

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "Constructors shall explicitly initialize all virtual base classes, all direct non-virtual base classes and all non-static data members.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A12_1_1 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      cxxConstructorDecl(unless(isImplicit())).bind(kCXXConstructorDeclString),
      this);
}

// The checker collects all direct non-virtual base classes, virtual base
// classes, and all member variables firstly, then it checks the init list of
// the constructor. If a base class or variable is initialized in the list, it's
// removed from the collection. Then the checker walks through the body of the
// constructor. For builtin type, if there exists an assignment operator, and
// the lhs is a reference to a member variable, the variable is considered as
// initialized. For cxx class, it also checks the cxx operator call expr. If the
// operator is assignment and the the second child, which is the lhs, is a
// member variable, the variable is still considered as initialized. After that,
// the checker checks whether the collection is empty. It reports an error if
// not.
void Callback::run(const MatchFinder::MatchResult& result) {
  const CXXConstructorDecl* cxx_constructor_decl =
      result.Nodes.getNodeAs<CXXConstructorDecl>(kCXXConstructorDeclString);
  const CXXRecordDecl* cxx_record_decl = cxx_constructor_decl->getParent();
  if (misra::libtooling_utils::IsInSystemHeader(cxx_constructor_decl,
                                                result.Context) ||
      misra::libtooling_utils::IsInSystemHeader(cxx_record_decl,
                                                result.Context)) {
    return;
  }
  std::set<const FieldDecl*> members = {};
  std::set<const CXXRecordDecl*> bases = {};
  for (const Decl* decl : cxx_record_decl->decls()) {
    if (const FieldDecl* field_decl = dyn_cast<FieldDecl>(decl)) {
      if (!field_decl->hasInClassInitializer()) {
        members.insert(field_decl);
      }
    }
  }
  for (const CXXBaseSpecifier& base : cxx_record_decl->bases()) {
    if (const CXXRecordDecl* base_decl = base.getType()->getAsCXXRecordDecl())
      bases.insert(base_decl);
  }
  for (const CXXRecordDecl* virtual_base_decl :
       FindVirtualBases(cxx_record_decl)) {
    bases.insert(virtual_base_decl);
  }
  for (const CXXCtorInitializer* init : cxx_constructor_decl->inits()) {
    if (init->isWritten()) {
      if (init->isMemberInitializer()) {
        members.erase(init->getMember());
      } else if (init->isBaseInitializer()) {
        bases.erase(init->getBaseClass()->getAsCXXRecordDecl());
      }
    }
  }
  if (cxx_constructor_decl->hasBody()) {
    if (const Stmt* body = cxx_constructor_decl->getBody()) {
      for (const Stmt* child : body->children()) {
        if (const BinaryOperator* binary_operator =
                dyn_cast<BinaryOperator>(child)) {
          if (binary_operator->isAssignmentOp()) {
            if (const MemberExpr* lhs = dyn_cast<MemberExpr>(
                    binary_operator->getLHS()->IgnoreCasts())) {
              if (const FieldDecl* field_decl =
                      dyn_cast<FieldDecl>(lhs->getMemberDecl())) {
                members.erase(field_decl);
              }
            }
          }
        } else if (const CXXOperatorCallExpr* cxx_operator_call_expr =
                       dyn_cast<CXXOperatorCallExpr>(child)) {
          if (cxx_operator_call_expr->isAssignmentOp()) {
            ConstStmtIterator member_it =
                ++cxx_operator_call_expr->child_begin();
            if (member_it != cxx_operator_call_expr->child_end()) {
              if (const MemberExpr* member = dyn_cast<MemberExpr>(*member_it)) {
                if (const FieldDecl* field_decl =
                        dyn_cast<FieldDecl>(member->getMemberDecl())) {
                  members.erase(field_decl);
                }
              }
            }
          }
        }
      }
    }
  }
  if (!members.empty() || !bases.empty()) {
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
}  // namespace rule_A12_1_1
}  // namespace autosar
