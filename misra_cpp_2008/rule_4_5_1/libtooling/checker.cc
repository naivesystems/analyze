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

#include "misra_cpp_2008/rule_4_5_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_4_5_1 {
namespace libtooling {
class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        binaryOperator(hasEitherOperand(castExpr(
                           hasSourceExpression(expr(hasType(booleanType()))))),
                       unless(hasAnyOperatorName("=", "&&", "||", "==", "!=")))
            .bind("op"),
        this);
    finder->addMatcher(
        unaryOperator(hasUnaryOperand(castExpr(
                          hasSourceExpression(expr(hasType(booleanType()))))),
                      unless(hasAnyOperatorName("!", "&")))
            .bind("op"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const Expr* e = result.Nodes.getNodeAs<Expr>("op");
    string error_message =
        "bool类型的表达式不得用作内建运算符的操作数，除了赋值运算符=，逻辑运算符&&、||、!，相等运算符==、!=，一元运算符&和条件运算符";
    string path = misra::libtooling_utils::GetFilename(e, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(e, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_4_5_1);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_4_5_1
}  // namespace misra_cpp_2008
