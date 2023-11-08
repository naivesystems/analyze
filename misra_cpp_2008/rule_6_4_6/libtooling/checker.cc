/*
Copyright 2022 Naive Systems Ltd.
This software contains information and intellectual property that is
confidential and proprietary to Naive Systems Ltd. and its affiliates.
*/

#include "misra_cpp_2008/rule_6_4_6/libtooling/checker.h"

#include <glog/logging.h>

#include <regex>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list,
                 string external_message = "") {
  string error_message = "switch语句的最后一个子句必须是default子句";
  auto result = misra::proto_util::AddResultToResultsList(
      results_list, path, line_number, error_message);

  if (!external_message.empty()) {
    result->set_external_message(external_message);
  }
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_6_4_6 {
namespace libtooling {

AST_MATCHER(Stmt, isNotNullStmt) {
  return Stmt::StmtClass::NullStmtClass != Node.getStmtClass();
}

AST_MATCHER(Stmt, isNotBreakStmt) {
  return Stmt::StmtClass::BreakStmtClass != Node.getStmtClass();
}

class InappropriateSwitchCallback
    : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        switchStmt(
            anyOf(has(compoundStmt(has(
                      defaultStmt(
                          anyOf(hasDescendant(
                                    stmt(isNotBreakStmt(), isNotNullStmt())
                                        .bind("any_stmt_in_default_clause")),
                                anything()))
                          .bind("default_clause")))),
                  anything()))
            .bind("switch_stmt"),
        this);
  }
  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    auto switch_stmt = result.Nodes.getNodeAs<SwitchStmt>("switch_stmt");

    if (misra::libtooling_utils::IsInSystemHeader(switch_stmt,
                                                  result.Context)) {
      return;
    }

    if (auto default_clause =
            result.Nodes.getNodeAs<DefaultStmt>("default_clause")) {
      if (switch_stmt->getSwitchCaseList() == default_clause) {
        if (result.Nodes.getNodeAs<Stmt>("any_stmt_in_default_clause")) {
          return;
        } else if (std::regex_match(
                       misra::libtooling_utils::GetTokenFromSourceLoc(
                           result.SourceManager, default_clause->getBeginLoc(),
                           switch_stmt->getEndLoc()),
                       std::regex("(.|\n)*(//|/[*])(.|\n)*"))) {
          // exception: default clause without appropriate action
          // but with a comment as to why no action is taken
          return;
        }
        ReportError(
            misra::libtooling_utils::GetFilename(switch_stmt,
                                                 result.SourceManager),
            misra::libtooling_utils::GetLine(switch_stmt, result.SourceManager),
            results_list_,
            "Invalid default statements without comments are not allowed");
        return;
      }
    } else if (switch_stmt->isAllEnumCasesCovered()) {
      return;
    }

    ReportError(
        misra::libtooling_utils::GetFilename(switch_stmt, result.SourceManager),
        misra::libtooling_utils::GetLine(switch_stmt, result.SourceManager),
        results_list_);

    return;
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new InappropriateSwitchCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_6_4_6
}  // namespace misra_cpp_2008
