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

#include "autosar/rule_A13_5_4/libtooling/checker.h"

#include <glog/logging.h>

#include <map>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang::ast_matchers;

namespace {
std::unordered_map<OverloadedOperatorKind, OverloadedOperatorKind>
    oppositeOperator;
std::unordered_map<const Decl*,
                   std::unordered_map<OverloadedOperatorKind, bool>>
    isDefinedByScope;  // recorder of whether a specific overloaded operator is
                       // declared within a specific scope

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "If two opposite operators are defined, one shall be defined in terms of the other.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

// find the closest ancestor declaration that serves as the scope for the
// operator function
const Decl* findAncestorDecl(const FunctionDecl* functionDecl) {
  const DeclContext* parentDeclContext = functionDecl->getLexicalParent();
  while (parentDeclContext) {
    if (const Decl* ancestorDecl = dyn_cast<Decl>(parentDeclContext)) {
      return ancestorDecl;
    }
    parentDeclContext = parentDeclContext->getParent();
  }
  return nullptr;
}

// check whether the body of the FunctionDecl matches the pattern !(param1 op
// param2)
const bool isCorrectPattern(const FunctionDecl* functionDecl,
                            OverloadedOperatorKind op) {
  const CompoundStmt* compoundStmt =
      dyn_cast<CompoundStmt>(functionDecl->getBody());
  if (!compoundStmt) {
    return false;
  }
  const ReturnStmt* returnStmt =
      dyn_cast<ReturnStmt>(*(compoundStmt->child_begin()));
  if (!returnStmt) {
    return false;
  }
  const UnaryOperator* unOp =
      dyn_cast<UnaryOperator>(*(returnStmt->child_begin()));
  if (!unOp || unOp->getOpcode() != UO_LNot) {
    return false;
  }
  const ParenExpr* parenExpr = dyn_cast<ParenExpr>(*(unOp->child_begin()));
  if (!parenExpr) {
    return false;
  }
  const CXXOperatorCallExpr* opCall =
      dyn_cast<CXXOperatorCallExpr>(*(parenExpr->child_begin()));
  if (!opCall || opCall->getOperator() != op) {
    return false;
  }
  int childIndex = 0;
  for (const Stmt* child : opCall->children()) {
    if (childIndex == 0) {
      continue;
    }
    const DeclRefExpr* ref = dyn_cast<DeclRefExpr>(child);
    if (!ref) {
      return false;
    }
    if (childIndex > functionDecl->getNumParams()) {
      return false;
    }
    const ParmVarDecl* param = functionDecl->getParamDecl(childIndex - 1);
    if (ref->getDecl() != param) {
      return false;
    }
    childIndex++;
  }
  return true;
}
}  // namespace

namespace autosar {
namespace rule_A13_5_4 {
namespace libtooling {

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(functionDecl(hasAnyOverloadedOperatorName(
                                      "==", "!=", "<=", ">=", ">", "<"))
                         .bind("function"),
                     this);
  oppositeOperator[OO_Less] = OO_GreaterEqual;
  oppositeOperator[OO_GreaterEqual] = OO_Less;
  oppositeOperator[OO_Greater] = OO_LessEqual;
  oppositeOperator[OO_LessEqual] = OO_Greater;
  oppositeOperator[OO_ExclaimEqual] = OO_EqualEqual;
  oppositeOperator[OO_EqualEqual] = OO_ExclaimEqual;
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const FunctionDecl* functionDecl =
      result.Nodes.getNodeAs<FunctionDecl>("function");
  if (misra::libtooling_utils::IsInSystemHeader(functionDecl, result.Context)) {
    return;
  }
  const OverloadedOperatorKind overloadedOperator =
      functionDecl->getOverloadedOperator();
  const Decl* ancestorDecl = findAncestorDecl(
      functionDecl);  // get the current scope of this functionDecl
  if (ancestorDecl) {
    const OverloadedOperatorKind overloadedOperator =
        functionDecl->getOverloadedOperator();
    if (!isCorrectPattern(functionDecl, oppositeOperator[overloadedOperator])) {
      isDefinedByScope[ancestorDecl][overloadedOperator] = true;
      if (isDefinedByScope[ancestorDecl]
                          [oppositeOperator[overloadedOperator]]) {
        // the opposite operator is defined in the current scope
        std::string path = misra::libtooling_utils::GetFilename(
            functionDecl, result.SourceManager);
        int line_number = misra::libtooling_utils::GetLine(
            functionDecl, result.SourceManager);
        ReportError(path, line_number, results_list_);
      }
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
}  // namespace rule_A13_5_4
}  // namespace autosar
