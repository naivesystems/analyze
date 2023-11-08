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

#include "misra/rule_13_5/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "clang/AST/Expr.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang::ast_matchers;
using namespace misra::proto_util;
using analyzer::proto::ResultsList;

namespace misra {
namespace rule_13_5 {

class CastCallback : public MatchFinder::MatchCallback {
 public:
  void Init(bool aggressive_mode, ResultsList* results_list,
            MatchFinder* finder) {
    results_list_ = results_list;
    aggressive_mode_ = aggressive_mode;
    finder->addMatcher(
        binaryOperator(anyOf(hasOperatorName("&&"), hasOperatorName("||")),
                       hasRHS(expr().bind("rhs"))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* rhs = result.Nodes.getNodeAs<Expr>("rhs");
    ASTContext* context = result.Context;
    clang::FullSourceLoc loc = context->getFullLoc(rhs->getBeginLoc());
    if (loc.isInvalid() || loc.isInSystemHeader()) {
      return;
    }
    string path = libtooling_utils::GetFilename(rhs, result.SourceManager);
    int line_number = libtooling_utils::GetLine(rhs, result.SourceManager);
    string location = libtooling_utils::GetLocation(rhs, result.SourceManager);
    if (!rhs->HasSideEffects(*context)) {
      return;
    }
    libtooling_utils::ConstCallExprVisitor Visitor(context);
    Visitor.Visit(rhs);
    if (!Visitor.ShouldReport(aggressive_mode_)) {
      return;
    }
    std::string error_message = absl::StrFormat(
        "[C1602][misra-c2012-13.5]: Right hand operand may have persistent side effect, Location: %s",
        location);
    analyzer::proto::Result* pb_result =
        AddResultToResultsList(results_list_, path, line_number, error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_C_2012_RULE_13_5);
    pb_result->set_loc(location);
    LOG(INFO) << error_message;
  }

 private:
  ResultsList* results_list_;
  bool aggressive_mode_;
};

void Checker::Init(bool aggressive_mode, ResultsList* results_list) {
  results_list_ = results_list;
  castCallback_ = new CastCallback;
  castCallback_->Init(aggressive_mode, results_list_, &finder_);
}

}  // namespace rule_13_5
}  // namespace misra
