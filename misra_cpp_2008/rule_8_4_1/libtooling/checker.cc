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

#include "misra_cpp_2008/rule_8_4_1/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "不应在函数定义中使用省略号";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_8_4_1 {
namespace libtooling {

void FuncDefEllipsisCallback::Init(ResultsList* results_list,
                                   MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(functionDecl(isDefinition()).bind("d"), this);
}

void FuncDefEllipsisCallback::run(const MatchFinder::MatchResult& result) {
  ASTContext* context = result.Context;

  const FunctionDecl* function_match =
      result.Nodes.getNodeAs<FunctionDecl>("d");

  if (function_match->getEllipsisLoc().isValid()) {
    std::string path = misra::libtooling_utils::GetFilename(
        function_match, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(function_match, result.SourceManager);
    ReportError(path, line_number, results_list_);
  }
}

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new FuncDefEllipsisCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_8_4_1
}  // namespace misra_cpp_2008
