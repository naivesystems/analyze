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

#include "googlecpp/g1204/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

// A `ReturnStmt` counter for function body
// Reference: Clang-Analyzer-Guide-v0.1.pdf, section 3.2.1.
class ReturnCounter : public ConstStmtVisitor<ReturnCounter> {
  void VisitChildren(const Stmt* S) {
    for (Stmt ::const_child_iterator I = S->child_begin(), E = S->child_end();
         I != E; ++I)
      if (const Stmt* Child = *I) Visit(Child);
  }

  int cnt = 0;
  int getCnt() { return cnt; }

 public:
  ReturnCounter() : cnt(0) {}
  void VisitStmt(const Stmt* S) {
    if (llvm::dyn_cast<ReturnStmt>(S)) cnt++;
    VisitChildren(S);
  }
  void VisitCallExpr(const CallExpr* CE) {
    // Don't do inter-procedural count, left blank as intended
  }
  static int getReturnCnt(const FunctionDecl* D) {
    ReturnCounter RC;
    if (D->getBody() == nullptr) return 0;
    RC.Visit(D->getBody());
    return RC.getCnt();
  }
};

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Return type deduction should only be used in small functions";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1204 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
  int maxAllowedReturnNum;
  int maxAllowedFuncLine;

 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder, int maxAllowedReturnNum,
            int maxAllowedFuncLine) {
    results_list_ = results_list;
    // It will also catch lambda functions
    finder->addMatcher(functionDecl().bind("func"), this);
    this->maxAllowedReturnNum = maxAllowedReturnNum;
    this->maxAllowedFuncLine = maxAllowedFuncLine;
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    if (const auto* func = result.Nodes.getNodeAs<FunctionDecl>("func")) {
      if (misra::libtooling_utils::IsInSystemHeader(func, result.Context))
        return;
      if (func->getDeclaredReturnType()->isUndeducedAutoType()) {
        int rangeLine = misra::libtooling_utils::GetRealLine(
                            func->getEndLoc(), result.SourceManager) -
                        misra::libtooling_utils::GetRealLine(
                            func->getBeginLoc(), result.SourceManager) +
                        1;
        int numReturn = ReturnCounter::getReturnCnt(func);

        if (rangeLine <= maxAllowedFuncLine && numReturn <= maxAllowedReturnNum)
          return;

        ReportError(
            misra::libtooling_utils::GetFilename(func, result.SourceManager),
            misra::libtooling_utils::GetLine(func, result.SourceManager),
            results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list,
                   int maxAllowedReturnNum, int maxAllowedFuncLine) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_, maxAllowedReturnNum,
                  maxAllowedFuncLine);
}
}  // namespace libtooling
}  // namespace g1204
}  // namespace googlecpp
