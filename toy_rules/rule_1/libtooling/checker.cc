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

#include "toy_rules/rule_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace toy_rules {
namespace rule_1 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        implicitCastExpr(hasSourceExpression(expr(gnuNullExpr())),
                         hasImplicitDestinationType(isInteger()),
                         unless(isExpansionInSystemHeader()))
            .bind("cast"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* expr = result.Nodes.getNodeAs<Expr>("cast");
    string error_message = "NULL不得用作整型值";
    string path =
        misra::libtooling_utils::GetFilename(expr, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(expr, result.SourceManager);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line, error_message);
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_1
}  // namespace toy_rules
