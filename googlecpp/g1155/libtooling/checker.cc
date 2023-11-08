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

#include "googlecpp/g1155/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Define functions inline only when they are small, say, 10 lines or fewer";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1155 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
  int maximum_inline_func_line_;

 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder, int maximum_inline_func_line) {
    results_list_ = results_list;
    maximum_inline_func_line_ = maximum_inline_func_line;
    // It will also catch the inline member functions
    finder->addMatcher(functionDecl(isInline()).bind("inlineFunc"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    if (const FunctionDecl* func =
            result.Nodes.getNodeAs<FunctionDecl>("inlineFunc")) {
      if (misra::libtooling_utils::IsInSystemHeader(func, result.Context))
        return;
      int rangeLine = misra::libtooling_utils::GetRealLine(
                          func->getEndLoc(), result.SourceManager) -
                      misra::libtooling_utils::GetRealLine(
                          func->getBeginLoc(), result.SourceManager) +
                      1;

      if (rangeLine <= maximum_inline_func_line_) return;

      ReportError(
          misra::libtooling_utils::GetFilename(func, result.SourceManager),
          misra::libtooling_utils::GetLine(func, result.SourceManager),
          results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list,
                   int maximum_inline_func_line) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_, maximum_inline_func_line);
}
}  // namespace libtooling
}  // namespace g1155
}  // namespace googlecpp
