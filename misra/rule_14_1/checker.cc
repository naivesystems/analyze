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

#include "misra/rule_14_1/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace {

void FloatLoopCounterErr(string path, int linenum, ResultsList* results_list) {
  string error_message =
      "[C1704][misra-c2012-14.1]: loop counter cannot be float";
  LOG(INFO) << error_message;
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, linenum, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_14_1);
}

}  // namespace

namespace misra {
namespace rule_14_1 {

class ForCounterCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(forStmt(hasLoopInit(stmt().bind("loop_init"))), this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Stmt* loop_init = result.Nodes.getNodeAs<Stmt>("loop_init");
    const ASTContext* context = result.Context;
    clang::FullSourceLoc location =
        context->getFullLoc(loop_init->getBeginLoc());
    if (location.isInvalid() || location.isInSystemHeader()) {
      return;
    }

    if (const DeclStmt* decl_init = dyn_cast<DeclStmt>(loop_init)) {
      if (!decl_init->isSingleDecl()) {
        for (auto it = decl_init->decl_begin(); it != decl_init->decl_end();
             ++it) {
          const ValueDecl* value_init = dyn_cast<ValueDecl>(*it);
          if (value_init->getType()->isFloatingType()) {
            FloatLoopCounterErr(
                libtooling_utils::GetFilename(loop_init, result.SourceManager),
                libtooling_utils::GetLine(loop_init, result.SourceManager),
                results_list_);
            break;
          }
        }
        return;
      }
      const ValueDecl* value_init =
          dyn_cast<ValueDecl>(decl_init->getSingleDecl());
      if (value_init->getType()->isFloatingType()) {
        FloatLoopCounterErr(
            libtooling_utils::GetFilename(loop_init, result.SourceManager),
            libtooling_utils::GetLine(loop_init, result.SourceManager),
            results_list_);
      }
    }

    if (const BinaryOperator* assign_init =
            dyn_cast<BinaryOperator>(loop_init)) {
      if (!assign_init->isAssignmentOp()) return;
      if (const DeclRefExpr* lhs_ref =
              dyn_cast<DeclRefExpr>(assign_init->getLHS())) {
        if (lhs_ref->getType()->isFloatingType()) {
          FloatLoopCounterErr(
              libtooling_utils::GetFilename(loop_init, result.SourceManager),
              libtooling_utils::GetLine(loop_init, result.SourceManager),
              results_list_);
        }
      }
    }
  }

 private:
  ResultsList* results_list_;
};

class WhileCounterCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        whileStmt(hasCondition(binaryOperator(hasLHS(expr().bind("lhs")),
                                              hasRHS(expr().bind("rhs")))
                                   .bind("op"))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* lhs = result.Nodes.getNodeAs<Expr>("lhs");
    const Expr* rhs = result.Nodes.getNodeAs<Expr>("rhs");
    const BinaryOperator* op = result.Nodes.getNodeAs<BinaryOperator>("op");
    const ASTContext* context = result.Context;

    if (!op->isComparisonOp()) return;

    if (const ImplicitCastExpr* lhs_cast = dyn_cast<ImplicitCastExpr>(lhs)) {
      if (const DeclRefExpr* lhs_ref =
              dyn_cast<DeclRefExpr>(lhs_cast->getSubExpr())) {
        if (lhs_ref->getDecl()->getType()->isFloatingType()) {
          FloatLoopCounterErr(
              libtooling_utils::GetFilename(lhs, result.SourceManager),
              libtooling_utils::GetLine(lhs, result.SourceManager),
              results_list_);
        }
      }
    }

    if (const ImplicitCastExpr* rhs_cast = dyn_cast<ImplicitCastExpr>(rhs)) {
      if (const DeclRefExpr* rhs_ref =
              dyn_cast<DeclRefExpr>(rhs_cast->getSubExpr())) {
        if (rhs_ref->getDecl()->getType()->isFloatingType()) {
          FloatLoopCounterErr(
              libtooling_utils::GetFilename(rhs, result.SourceManager),
              libtooling_utils::GetLine(rhs, result.SourceManager),
              results_list_);
        }
      }
    }
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  for_callback_ = new ForCounterCallback;
  for_callback_->Init(results_list_, &finder_);
  while_callback_ = new WhileCounterCallback;
  while_callback_->Init(results_list_, &finder_);
}

}  // namespace rule_14_1
}  // namespace misra
