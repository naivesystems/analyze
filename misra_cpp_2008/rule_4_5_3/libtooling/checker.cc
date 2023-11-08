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

#include "misra_cpp_2008/rule_4_5_3/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;

namespace misra_cpp_2008 {
namespace rule_4_5_3 {
namespace libtooling {
const int zero_char = 48;
class OPCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        binaryOperator(
            hasEitherOperand(castExpr(hasSourceExpression(anyOf(
                hasType(asString("char")), hasType(asString("wchar_t")))))),
            unless(hasAnyOperatorName("=", "==", "!=", "+", "-", "<", "<=", ">",
                                      ">=")))
            .bind("op"),
        this);
    // match binary + operator with no operand '0'
    finder->addMatcher(
        binaryOperator(
            hasOperatorName("+"),
            hasEitherOperand(castExpr(hasSourceExpression(anyOf(
                hasType(asString("char")), hasType(asString("wchar_t")))))),
            unless(hasEitherOperand(castExpr(
                hasSourceExpression(characterLiteral(equals(zero_char)))))))
            .bind("op"),
        this);
    // match binary - operator whose RHS is not '0'
    finder->addMatcher(
        binaryOperator(
            hasOperatorName("-"),
            hasEitherOperand(castExpr(hasSourceExpression(anyOf(
                hasType(asString("char")), hasType(asString("wchar_t")))))),
            unless(hasRHS(castExpr(
                hasSourceExpression(characterLiteral(equals(zero_char)))))))
            .bind("op"),
        this);
    // match relational operators with no operand '0' or '9'
    finder->addMatcher(
        binaryOperator(
            hasAnyOperatorName("<", "<=", ">", ">="),
            hasEitherOperand(castExpr(hasSourceExpression(anyOf(
                hasType(asString("char")), hasType(asString("wchar_t")))))),
            unless(hasEitherOperand(castExpr(hasSourceExpression(
                anyOf(characterLiteral(equals(zero_char)),
                      characterLiteral(equals(zero_char + 9))))))))
            .bind("op"),
        this);
    finder->addMatcher(
        unaryOperator(
            hasUnaryOperand(castExpr(hasSourceExpression(anyOf(
                hasType(asString("char")), hasType(asString("wchar_t")))))),
            unless(hasOperatorName("&")))
            .bind("op"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const Expr* e = result.Nodes.getNodeAs<Expr>("op");
    if (misra::libtooling_utils::IsInSystemHeader(e, result.Context)) {
      return;
    }
    string error_message =
        "类型为（普通）char和wchar_t的表达式不得用作内建运算符的操作数，除了赋值运算符=，相等运算符==和!=，以及一元运算符&";
    string path = misra::libtooling_utils::GetFilename(e, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(e, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_4_5_3);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

class AddCallback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        binaryOperator(
            hasOperatorName("+"),
            hasLHS(ignoringImpCasts(expr(hasType(isInteger())).bind("int"))),
            hasRHS(castExpr(
                hasSourceExpression(characterLiteral(equals(zero_char))))))
            .bind("addOp"),
        this);
    finder->addMatcher(
        binaryOperator(
            hasOperatorName("+"),
            hasRHS(ignoringImpCasts(expr(hasType(isInteger())).bind("int"))),
            hasLHS(castExpr(
                hasSourceExpression(characterLiteral(equals(zero_char))))))
            .bind("addOp"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    const BinaryOperator* add_op =
        result.Nodes.getNodeAs<BinaryOperator>("addOp");
    const Expr* int_operand = result.Nodes.getNodeAs<Expr>("int");

    if (misra::libtooling_utils::IsInSystemHeader(add_op, result.Context)) {
      return;
    }

    if (int_operand->getType()->isIntegerType() &&
        !int_operand->isValueDependent()) {
      if (!int_operand->isEvaluatable(*context)) {
        return;
      }
      clang::Expr::EvalResult rint;
      int_operand->EvaluateAsInt(rint, *context);
      if (rint.Val.isInt()) {
        if (rint.Val.getInt() >= 0 && rint.Val.getInt() <= 9) {
          return;
        }
      }
    }
    string error_message =
        "类型为（普通）char和wchar_t的表达式不得用作内建运算符的操作数，除了赋值运算符=，相等运算符==和!=，以及一元运算符&";
    string path =
        misra::libtooling_utils::GetFilename(add_op, result.SourceManager);
    int line = misra::libtooling_utils::GetLine(add_op, result.SourceManager);
    analyzer::proto::Result* pb_result =
        misra::proto_util::AddResultToResultsList(results_list_, path, line,
                                                  error_message);
    pb_result->set_error_kind(
        analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_4_5_3);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  op_callback_ = new OPCallback;
  add_callback_ = new AddCallback;
  op_callback_->Init(result_list, &finder_);
  add_callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace rule_4_5_3
}  // namespace misra_cpp_2008
