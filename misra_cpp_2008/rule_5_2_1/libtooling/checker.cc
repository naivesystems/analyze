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

#include "misra_cpp_2008/rule_5_2_1/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "[misra_cpp_2008-5.2.1] 逻辑运算符&&或||的每个操作数都必须是后缀表达式";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_2_1 {
namespace libtooling {

// Ignore MaterializeTemporaryExpr, it occurs in overload operator
// functions whose return value is a user defined type
AST_MATCHER_P(Expr, ignoringMaterializeTemporaryExpr,
              ast_matchers::internal::Matcher<Expr>, InnerMatcher) {
  const Expr* E = &Node;
  if (const auto* MaterializeTemp = dyn_cast<MaterializeTemporaryExpr>(E)) {
    return InnerMatcher.matches(*MaterializeTemp->getSubExpr(), Finder,
                                Builder);
  }
  return InnerMatcher.matches(Node, Finder, Builder);
}

StringRef getBinOpName(const Expr* binOp_) {
  if (const BinaryOperator* bo = dyn_cast<BinaryOperator>(binOp_)) {
    return bo->getOpcodeStr();
  } else if (const CXXOperatorCallExpr* cxx =
                 dyn_cast<CXXOperatorCallExpr>(binOp_)) {
    return getOpName(*cxx).hasValue() ? getOpName(*cxx).value() : "";
  }
  return "";
}

void CheckLogicOperatorCallback::Init(ResultsList* results_list,
                                      MatchFinder* finder) {
  results_list_ = results_list;

  // binaryOperation matches:
  // BinaryOperator, CXXOperatorCallExpr, CXXRewrittenBinaryOperator
  // skip CXXRewrittenBinaryOperator becauses it only
  // works with operator "==", "!=" and "<=, >, >=, <=>".
  auto shouldEnclose =
      anyOf(binaryOperation(), unaryOperator(), conditionalOperator());
  finder->addMatcher(
      binaryOperation(hasAnyOperatorName("&&", "||"),
                      hasEitherOperand(ignoringMaterializeTemporaryExpr(
                          ignoringImpCasts(expr(shouldEnclose).bind("child")))))
          .bind("operation"),
      this);
}

void CheckLogicOperatorCallback::run(const MatchFinder::MatchResult& result) {
  const Expr* binOp_ = result.Nodes.getNodeAs<Expr>("operation");
  const Expr* child_ = result.Nodes.getNodeAs<Expr>("child");

  if (misra::libtooling_utils::IsInSystemHeader(binOp_, result.Context)) {
    return;
  }

  // getBinOpName(binOp_) always returns "&&" or "||"
  // getBinOpName(child_) returns "" when it is not a binaryOp
  if (getBinOpName(binOp_) == getBinOpName(child_)) {
    return;
  }

  ReportError(
      misra::libtooling_utils::GetFilename(binOp_, result.SourceManager),
      misra::libtooling_utils::GetLine(binOp_, result.SourceManager),
      results_list_);
  return;
}

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckLogicOperatorCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_2_1
}  // namespace misra_cpp_2008
