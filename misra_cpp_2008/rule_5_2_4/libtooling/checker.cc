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

#include "misra_cpp_2008/rule_5_2_4/libtooling/checker.h"

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
      "[misra_cpp_2008-5.2.4]: 不得使用C风格的转换（除了void转换）和函数式记法转换（除了显式构造函数调用）";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_5_2_4 {
namespace libtooling {

AST_MATCHER(QualType, isVoid) { return Node->isVoidType(); }

void CheckCastCallback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      explicitCastExpr(
          anyOf(
              cxxFunctionalCastExpr(unless(hasDescendant(cxxConstructExpr()))),
              cStyleCastExpr(unless(hasDestinationType(qualType(isVoid()))))))
          .bind("cast"),
      this);
}

void CheckCastCallback::run(const MatchFinder::MatchResult& result) {
  const ExplicitCastExpr* cast_ =
      result.Nodes.getNodeAs<ExplicitCastExpr>("cast");

  if (misra::libtooling_utils::IsInSystemHeader(cast_, result.Context)) {
    return;
  }

  ReportError(misra::libtooling_utils::GetFilename(cast_, result.SourceManager),
              misra::libtooling_utils::GetLine(cast_, result.SourceManager),
              results_list_);
  return;
}

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  record_callback_ = new CheckCastCallback;
  record_callback_->Init(result_list, &finder_);
}

}  // namespace libtooling
}  // namespace rule_5_2_4
}  // namespace misra_cpp_2008
