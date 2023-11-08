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

#include "misra/rule_17_8/checker.h"

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
namespace rule_17_8 {

class ExprCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(binaryOperator(isAssignmentOperator(),
                                      hasLHS(declRefExpr(to(parmVarDecl()))))
                           .bind("expr"),
                       this);
    finder->addMatcher(
        unaryOperator(anyOf(hasOperatorName("++"), hasOperatorName("--")),
                      hasUnaryOperand(declRefExpr(to(parmVarDecl()))))
            .bind("expr"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* expr = result.Nodes.getNodeAs<Expr>("expr");
    if (libtooling_utils::IsInSystemHeader(expr, result.Context)) {
      return;
    }

    std::string error_message =
        "[C1501][misra-c2012-17.8]: parameters should not be modified";
    analyzer::proto::Result* pb_result = AddResultToResultsList(
        results_list_,
        libtooling_utils::GetFilename(expr, result.SourceManager),
        libtooling_utils::GetLine(expr, result.SourceManager), error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_17_8);
    LOG(INFO) << error_message;
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new ExprCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace rule_17_8
}  // namespace misra
