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

#include "misra_cpp_2008/rule_18_4_1/libtooling/checker.h"

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
  string error_message = "不应使用动态堆内存分配";
  AddResultToResultsList(results_list, path, line_number, error_message);
}

}  // namespace

namespace misra_cpp_2008 {
namespace rule_18_4_1 {
namespace libtooling {

class SpecificFunctionUsageCallback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(cxxNewExpr().bind("new"), this);
    finder->addMatcher(cxxDeleteExpr().bind("delete"), this);
    finder->addMatcher(declRefExpr(hasType(functionType())).bind("func_ptr"),
                       this);
  }
  void run(const MatchFinder::MatchResult& result) override {
    const CXXNewExpr* new_expr = result.Nodes.getNodeAs<CXXNewExpr>("new");
    const CXXDeleteExpr* delete_expr =
        result.Nodes.getNodeAs<CXXDeleteExpr>("delete");
    const DeclRefExpr* func_ptr =
        result.Nodes.getNodeAs<DeclRefExpr>("func_ptr");

    Stmt const* error_stmt = nullptr;

    if (nullptr != new_expr) {
      error_stmt = new_expr;
    }
    if (nullptr != delete_expr) {
      error_stmt = delete_expr;
    }
    if (nullptr != func_ptr) {
      string func_name = func_ptr->getNameInfo().getName().getAsString();
      // 参考 https://zh.cppreference.com/w/c/experimental/dynamic
      if (std::regex_match(
              func_name,
              std::regex("(m|re|c)alloc|free|getw?line|getw?delim|strn?dup")) &&
          misra::libtooling_utils::IsInSystemHeader(func_ptr->getFoundDecl(),
                                                    result.Context)) {
        error_stmt = func_ptr;
      }
    }
    if (nullptr != error_stmt && !misra::libtooling_utils::IsInSystemHeader(
                                     error_stmt, result.Context)) {
      ReportError(
          misra::libtooling_utils::GetFilename(error_stmt,
                                               result.SourceManager),
          misra::libtooling_utils::GetLine(error_stmt, result.SourceManager),
          results_list_);
    }

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
}  // namespace rule_18_4_1
}  // namespace misra_cpp_2008
