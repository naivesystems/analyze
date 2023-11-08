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

#include "misra/dir_4_7/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra {
namespace dir_4_7 {

class CallParamCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        compoundStmt(
            has(callExpr(
                    forEachArgumentWithParam(
                        declRefExpr(to(varDecl(matchesName("success|ok")))),
                        parmVarDecl(anyOf(hasType(referenceType()),
                                          hasType(pointerType())))))
                    .bind("call")),
            unless(has(ifStmt(hasCondition(expr(hasDescendant(
                declRefExpr(to(varDecl(matchesName("success|ok"))))))))))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* call = result.Nodes.getNodeAs<Expr>("call");

    std::string error_message =
        "[C2310][misra-c2012-dir-4.7]: error should be tested";
    analyzer::proto::Result* pb_result = AddResultToResultsList(
        results_list_,
        libtooling_utils::GetFilename(call, result.SourceManager),
        libtooling_utils::GetLine(call, result.SourceManager), error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_DIR_4_7_ERROR_SHOULD_BE_TESTED);
    LOG(INFO) << error_message;
  }

 private:
  ResultsList* results_list_;
};

class CallAssignCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto function_with_err =
        functionDecl(hasDescendant(returnStmt(hasReturnValue(implicitCastExpr(
            has(declRefExpr(to(varDecl(matchesName(".*err.*"))))))))));
    finder->addMatcher(
        compoundStmt(
            has(binaryOperation(
                    isAssignmentOperator(), hasLHS(declRefExpr(to(varDecl()))),
                    hasRHS(
                        anyOf(callExpr(callee(function_with_err)),
                              expr(has(callExpr(callee(function_with_err)))))))
                    .bind("call")),
            unless(has(ifStmt(hasCondition(
                expr(hasDescendant(declRefExpr(to(varDecl()))))))))),
        this);
    finder->addMatcher(
        compoundStmt(
            has(binaryOperation(
                    isAssignmentOperator(),
                    hasLHS(declRefExpr(to(varDecl(matchesName("success|ok"))))),
                    hasRHS(anyOf(callExpr(), expr(has(callExpr())))))
                    .bind("call")),
            unless(has(ifStmt(hasCondition(expr(hasDescendant(
                declRefExpr(to(varDecl(matchesName("success|ok"))))))))))),
        this);
    // Example: string str = fn() where fn returns a string by value
    finder->addMatcher(
        compoundStmt(
            has(exprWithCleanups(has(
                binaryOperation(
                    isAssignmentOperator(),
                    hasLHS(declRefExpr(to(varDecl(matchesName("success|ok"))))),
                    hasRHS(anyOf(callExpr(), expr(has(callExpr())),
                                 materializeTemporaryExpr(has(
                                     cxxBindTemporaryExpr(has(callExpr())))))))
                    .bind("call")))),
            unless(has(ifStmt(hasCondition(expr(hasDescendant(
                declRefExpr(to(varDecl(matchesName("success|ok"))))))))))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* call = result.Nodes.getNodeAs<Expr>("call");

    std::string error_message =
        "[C2310][misra-c2012-dir-4.7]: error should be tested";
    analyzer::proto::Result* pb_result = AddResultToResultsList(
        results_list_,
        libtooling_utils::GetFilename(call, result.SourceManager),
        libtooling_utils::GetLine(call, result.SourceManager), error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_DIR_4_7_ERROR_SHOULD_BE_TESTED);
    LOG(INFO) << error_message;
  }

 private:
  ResultsList* results_list_;
};

class CallCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    auto function_with_err =
        functionDecl(hasDescendant(returnStmt(hasReturnValue(implicitCastExpr(
            has(declRefExpr(to(varDecl(matchesName(".*err.*"))))))))));
    finder->addMatcher(
        callExpr(
            callee(function_with_err),
            unless(anyOf(hasAncestor(binaryOperator(isAssignmentOperator())),
                         hasAncestor(expr(hasParent(ifStmt()))))))
            .bind("call"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const CallExpr* call = result.Nodes.getNodeAs<CallExpr>("call");

    std::string error_message =
        "[C2310][misra-c2012-dir-4.7]: error should be tested";
    analyzer::proto::Result* pb_result = AddResultToResultsList(
        results_list_,
        libtooling_utils::GetFilename(call, result.SourceManager),
        libtooling_utils::GetLine(call, result.SourceManager), error_message);
    pb_result->set_error_kind(
        analyzer::proto::
            Result_ErrorKind_MISRA_C_2012_DIR_4_7_ERROR_SHOULD_BE_TESTED);
    LOG(INFO) << error_message;
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  call_callback_ = new CallCallback;
  call_callback_->Init(results_list_, &finder_);
  call_assign_callback = new CallAssignCallback;
  call_assign_callback->Init(results_list_, &finder_);
  call_param_callback_ = new CallParamCallback;
  call_param_callback_->Init(results_list_, &finder_);
}

}  // namespace dir_4_7
}  // namespace misra
