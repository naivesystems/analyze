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

#include "misra_cpp_2008/rule_6_2_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "不得直接或间接地对浮点表达式进行相等性或不等性测试";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_6_2_2 {
namespace libtooling {

void CheckFloatComparisonCallback::Init(ResultsList* results_list,
                                        MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      binaryOperator(
          hasAnyOperatorName("==", "!=", ">", "<", ">=", "<="),
          hasLHS(expr(hasType(hasCanonicalType(type().bind("lhs_type"))))),
          hasRHS(expr(hasType(hasCanonicalType(type().bind("rhs_type"))))),
          anyOf(has(callExpr(has(ignoringImpCasts(declRefExpr(
                    to(cxxMethodDecl(hasName("epsilon")).bind("epsilon"))))))),
                anything()))
          .bind("comparison"),
      this);
}

void CheckFloatComparisonCallback::run(const MatchFinder::MatchResult& result) {
  auto op_ = result.Nodes.getNodeAs<BinaryOperator>("comparison");
  auto lhs_type_ = result.Nodes.getNodeAs<Type>("lhs_type");
  auto rhs_type_ = result.Nodes.getNodeAs<Type>("rhs_type");
  auto epsilon = result.Nodes.getNodeAs<CXXMethodDecl>("epsilon");

  if (misra::libtooling_utils::IsInSystemHeader(op_, result.Context)) {
    return;
  }
  if (epsilon &&
      misra::libtooling_utils::IsInSystemHeader(epsilon, result.Context)) {
    return;
  }
  if (lhs_type_->isRealFloatingType() && rhs_type_->isRealFloatingType()) {
    ReportError(misra::libtooling_utils::GetFilename(op_, result.SourceManager),
                misra::libtooling_utils::GetLine(op_, result.SourceManager),
                results_list_);
  }

  return;
}

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new CheckFloatComparisonCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_6_2_2
}  // namespace misra_cpp_2008
