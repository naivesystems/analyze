/*
NaiveSystems Analyze - A tool for static code analysis
Copyright (C) 2022-2023  Naive Systems Ltd.

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

#include "misra/libtooling_utils/libtooling_utils.h"
#include "misra/proto_util.h"
#include "sdk/checker/ast_checker.h"
#include "sdk/checker/define_ast_checker.h"

using namespace clang::ast_matchers;

namespace {

class Callback : public sdk::checker::ASTCheckerCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            MatchFinder* finder) override {
    results_list_ = results_list;
    finder->addMatcher(castExpr(hasCastKind(CK_NullToPointer),
                                hasSourceExpression(integerLiteral(equals(0))))
                           .bind("cast"),
                       this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const Expr* e = result.Nodes.getNodeAs<Expr>("cast");
    if (misra::libtooling_utils::IsInSystemHeader(e, result.Context)) {
      return;
    }
    string error_message = "字面量零（0）不应用作空指针常量";
    string path = misra::libtooling_utils::GetFilename(e, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(e, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_4_10_2);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

sdk::checker::DefineASTChecker<Callback> _("misra_cpp_2008/rule_4_10_2");

}  // namespace
