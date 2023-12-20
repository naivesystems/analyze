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

#include "misra_cpp_2008/rule_5_0_15/libtooling/checker.h"

#include <clang/Lex/Lexer.h>
#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using namespace misra::libtooling_utils;
using analyzer::proto::ResultsList;

namespace {
void ReportError(ResultsList* results_list, string path, int line_number) {
  string error_message = "指针算术只得以数组索引的形式进行";
  analyzer::proto::Result* pb_result =
      AddResultToResultsList(results_list, path, line_number, error_message);
  pb_result->set_error_kind(
      analyzer::proto::Result_ErrorKind_MISRA_CPP_2008_RULE_5_0_15);
}
}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_0_15 {
namespace libtooling {

class BinaryOpCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        binaryOperator(hasEitherOperand(expr(hasType(pointerType()))))
            .bind("op"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const BinaryOperator* op = result.Nodes.getNodeAs<BinaryOperator>("op");
    if (IsInSystemHeader(op, result.Context)) {
      return;
    }
    if (!op->isAdditiveOp()) {
      return;
    }
    ReportError(results_list_, GetFilename(op, result.SourceManager),
                GetLine(op, result.SourceManager));
  }

 private:
  ResultsList* results_list_;
};

class ArraySubscriptCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        arraySubscriptExpr(hasBase(
            expr(hasType(pointerType()),
                 unless(castExpr(hasSourceExpression(hasType(arrayType())))))
                .bind("base"))),
        this);
  }

  void run(const MatchFinder::MatchResult& result) {
    const Expr* base = result.Nodes.getNodeAs<Expr>("base");
    if (IsInSystemHeader(base, result.Context)) {
      return;
    }
    ReportError(results_list_, GetFilename(base, result.SourceManager),
                GetLine(base, result.SourceManager));
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  bi_callback_ = new BinaryOpCallback;
  as_callback_ = new ArraySubscriptCallback;
  bi_callback_->Init(results_list_, &finder_);
  as_callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_0_15
}  // namespace misra_cpp_2008
