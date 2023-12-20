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
  auto result =
      AddResultToResultsList(results_list, path, line_number, error_message);

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

class InappropriateSwitchCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
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
  void run(const MatchFinder::MatchResult& result) override {
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
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new InappropriateSwitchCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_6_4_6
}  // namespace misra_cpp_2008
