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

#include "autosar/rule_A13_1_3/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <queue>
#include <vector>

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
      "User defined literals operators shall only perform conversion of passed parameters.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A13_1_3 {
namespace libtooling {

bool isLiteralOperatorFunction(const FunctionDecl* FD) {
  return FD->getDeclName().getNameKind() ==
         DeclarationName::CXXLiteralOperatorName;
}

bool hasSideEffectsInFunction(const FunctionDecl* FD,
                              const ASTContext& context) {
  queue<const Stmt*> exprQueue;

  for (auto child : FD->getBody()->children()) {
    exprQueue.push(child);
  }
  while (!exprQueue.empty()) {
    auto stmt = exprQueue.front();
    exprQueue.pop();
    if (isa<Expr>(stmt)) {
      const Expr* expr = cast<Expr>(stmt);
      bool hasPossibleSideEffects = expr->HasSideEffects(context);
      if (hasPossibleSideEffects) {
        return true;
      }
    } else if (!isa<ReturnStmt>(stmt)) {
      for (auto child : stmt->children()) {
        exprQueue.push(child);
      }
    }
  }
  return false;
}

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(functionDecl(hasDescendant(returnStmt()),
                                    unless(isExpansionInSystemHeader()))
                           .bind("decl"),
                       this);
    finder->addMatcher(functionDecl(unless(hasDescendant(returnStmt())),
                                    unless(isExpansionInSystemHeader()))
                           .bind("no_return_decl"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const FunctionDecl* decl = result.Nodes.getNodeAs<FunctionDecl>("decl");
    const FunctionDecl* no_return_decl =
        result.Nodes.getNodeAs<FunctionDecl>("no_return_decl");
    // if no return stmt, it does not perform conversion
    if (no_return_decl) {
      FullSourceLoc location =
          result.Context->getFullLoc(no_return_decl->getBeginLoc());
      if (location.isInvalid()) {
        return;
      }
      if (isLiteralOperatorFunction(no_return_decl)) {
        std::string path = misra::libtooling_utils::GetFilename(
            no_return_decl, result.SourceManager);
        int line_number = misra::libtooling_utils::GetLine(
            no_return_decl, result.SourceManager);
        ReportError(path, line_number, results_list_);
      }
      return;
    }

    if (!isLiteralOperatorFunction(decl) || !decl->hasBody()) {
      return;
    }

    // if its return type is void, it does not perform conversion
    // check side effect
    if (decl->getReturnType()->isVoidType() ||
        hasSideEffectsInFunction(decl, *(result.Context))) {
      std::string path =
          misra::libtooling_utils::GetFilename(decl, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(decl, result.SourceManager);
      ReportError(path, line_number, results_list_);
      return;
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
}  // namespace rule_A13_1_3
}  // namespace autosar
