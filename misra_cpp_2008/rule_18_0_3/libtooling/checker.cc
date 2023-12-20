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

#include "misra_cpp_2008/rule_18_0_3/libtooling/checker.h"

#include <glog/logging.h>

#include <regex>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "不应使用库 <cstdlib> 中的库函数 abort、exit、getenv 和 system";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_18_0_3 {
namespace libtooling {

class SpecificFunctionUsageCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(declRefExpr(hasType(functionType())).bind("func_ptr"),
                       this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const DeclRefExpr* func_ptr =
        result.Nodes.getNodeAs<DeclRefExpr>("func_ptr");

    string func_name = func_ptr->getNameInfo().getName().getAsString();

    if (!std::regex_match(func_name, std::regex("abort|exit|getenv|system"))) {
      return;
    }
    if (!misra::libtooling_utils::IsInSystemHeader(func_ptr->getFoundDecl(),
                                                   result.Context)) {
      return;
    }
    string decl_filename = misra::libtooling_utils::GetFilename(
        func_ptr->getFoundDecl(), result.SourceManager);
    // Since some library header like QT also in system header path, we need
    // extra check to avoid false positive.
    if (decl_filename.find("stdlib") == std::string::npos) {
      return;
    }
    ReportError(
        misra::libtooling_utils::GetFilename(func_ptr, result.SourceManager),
        misra::libtooling_utils::GetLine(func_ptr, result.SourceManager),
        results_list_);

    return;
  }

 private:
  ResultsList* results_list_;
};

void Checker::Init(ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new SpecificFunctionUsageCallback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_18_0_3
}  // namespace misra_cpp_2008
