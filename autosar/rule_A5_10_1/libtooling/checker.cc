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

#include "autosar/rule_A5_10_1/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A pointer to member virtual function shall only be tested for equality with null-pointer-constant.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A5_10_1 {
namespace libtooling {

bool isVirtualMemberFunctionPointer(const Expr* expr, const DeclRefExpr* ref) {
  if (expr->getType()->isMemberFunctionPointerType()) {
    const ValueDecl* decl = ref->getDecl();
    if (isa<CXXMethodDecl>(decl) && cast<CXXMethodDecl>(decl)->isVirtual()) {
      return true;
    }
  }
  return false;
};

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        binaryOperator(anyOf(hasOperatorName("=="), hasOperatorName("!=")),
                       unless(isExpansionInSystemHeader()),
                       hasLHS(hasDescendant(declRefExpr().bind("left"))),
                       hasRHS(hasDescendant(declRefExpr().bind("right"))))
            .bind("stmt"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const BinaryOperator* stmt = result.Nodes.getNodeAs<BinaryOperator>("stmt");

    const DeclRefExpr* left = result.Nodes.getNodeAs<DeclRefExpr>("left");
    const DeclRefExpr* right = result.Nodes.getNodeAs<DeclRefExpr>("right");

    const Expr* lhs = stmt->getLHS();
    const Expr* rhs = stmt->getRHS();

    if (left) {
      if (isVirtualMemberFunctionPointer(lhs, left)) {
        if (!isa<CXXNullPtrLiteralExpr>(rhs)) {
          std::string path =
              misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
          int line_number =
              misra::libtooling_utils::GetLine(stmt, result.SourceManager);
          ReportError(path, line_number, results_list_);
        }
        return;  // if rhs is nullptr, then return;
                 // if not, report error and then return
      }
    }

    if (right) {
      if (isVirtualMemberFunctionPointer(rhs, right)) {
        if (!isa<CXXNullPtrLiteralExpr>(lhs)) {
          std::string path =
              misra::libtooling_utils::GetFilename(stmt, result.SourceManager);
          int line_number =
              misra::libtooling_utils::GetLine(stmt, result.SourceManager);
          ReportError(path, line_number, results_list_);
        }
        return;
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A5_10_1
}  // namespace autosar
