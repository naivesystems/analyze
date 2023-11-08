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

#include "misra_cpp_2008/rule_6_5_4/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace misra::libtooling_utils;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_6_5_4 {
namespace libtooling {
class ForStmtCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    // match illegal loop-counter assignment
    // e.g. `i *= 2`, `i = 2`, `i /= 2`
    auto illegal_counter_assign_matcher = CreateAssignmentMatcher(
        ForIncrementVarFormat(BinaryOtherOpt | FunctionChange), "loop_counter",
        "", "modify_expr");

    // match increment and decrement binary operator expression
    // e.g. `i += n`, `i -= n`, `n` can be anything
    auto inde_bin_counter_matcher =
        CreateLoopCounterMatcher(AllCondFormat, BinaryInDecrease);

    // match loop counter (details can be seen in rule 6.5.1)
    auto loop_counter_matcher =
        CreateLoopCounterMatcher(AllCondFormat, AllIncFormat);

    // match a varaiable
    auto var_matcher =
        ignoringParenImpCasts(declRefExpr(to(varDecl().bind("n"))));

    // match varaible assignment
    // e.g.
    // for (int i = 0; i < 10; i += n) {
    //   n += 10;  // match the modification of n
    // }
    auto var_assign_matcher =
        CreateAssignmentMatcher(AllIncFormat, "n", "", "modify_expr");

    // find the `DeclRefExpr` of loop counter
    auto find_counter_matcher =
        declRefExpr(to(varDecl(equalsBoundNode("loop_counter"))));

    // find `x += n` or `x -= n` where n is a variable
    auto inde_bin_opt_with_ref_matcher = anyOf(
        findAll(binaryOperator(hasAnyOperatorName("+=", "-="),
                               hasOperands(find_counter_matcher, var_matcher))),
        findAll(cxxOperatorCallExpr(
            hasAnyOperatorName("+=", "-="),
            hasOperands(find_counter_matcher, var_matcher))));

    // find `x += n` or `x -= n`, where n is a constant
    // e.g. `x += 1`, `x -= 0.1`
    auto inde_bin_opt_with_const_matcher =
        anyOf(findAll(binaryOperator(
                  hasAnyOperatorName("+=", "-="),
                  hasOperands(find_counter_matcher,
                              anyOf(integerLiteral(), floatLiteral())))),
              findAll(cxxOperatorCallExpr(
                  hasAnyOperatorName("+=", "-="),
                  hasOperands(find_counter_matcher,
                              anyOf(integerLiteral(), floatLiteral())))));

    // match illegal loop-counter assignment in increment and body part
    finder->addMatcher(
        forStmt(loop_counter_matcher,
                eachOf(hasBody(illegal_counter_assign_matcher),
                       hasIncrement(illegal_counter_assign_matcher)))
            .bind("for_stmt"),
        this);

    // match `i += n`, where `n` is `DeclRefExpr`
    // e.g. `i += f()`
    finder->addMatcher(
        forStmt(inde_bin_counter_matcher,
                eachOf(hasIncrement(inde_bin_opt_with_ref_matcher),
                       hasBody(inde_bin_opt_with_ref_matcher)),
                hasBody(var_assign_matcher))
            .bind("for_stmt"),
        this);

    // match `i += n`, where `n` is not a `DeclRefExpr` or a constant, it might
    // be a function call or any other expressions
    finder->addMatcher(
        forStmt(inde_bin_counter_matcher,
                unless(anyOf(hasIncrement(anyOf(inde_bin_opt_with_const_matcher,
                                                inde_bin_opt_with_ref_matcher)),
                             hasBody(anyOf(inde_bin_opt_with_const_matcher,
                                           inde_bin_opt_with_ref_matcher)))))
            .bind("for_stmt"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const Stmt* for_stmt = result.Nodes.getNodeAs<Stmt>("for_stmt");
    const DeclRefExpr* modify_expr =
        result.Nodes.getNodeAs<DeclRefExpr>("modify_expr");

    if (IsInSystemHeader(for_stmt, result.Context)) {
      return;
    }
    string error_message =
        "循环计数器应通过以下方式之一进行修改：--、++、-=n或+=n；其中n在循环过程中保持不变";
    string path;
    int line = 0;
    if (modify_expr != nullptr) {
      path = GetFilename(modify_expr, result.SourceManager);
      line = GetLine(modify_expr, result.SourceManager);
    } else {
      path = GetFilename(for_stmt, result.SourceManager);
      line = GetLine(for_stmt, result.SourceManager);
    }
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_6_5_4);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  forStmtCallback_ = new ForStmtCallback;
  forStmtCallback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_6_5_4
}  // namespace misra_cpp_2008
