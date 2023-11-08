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

#include "googlecpp/g1166/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "Use of dynamic initialization for static class member variables or variables at namespace scope is discouraged";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1166 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    // Dynamic initialization of static local variables is allowed
    finder->addMatcher(
        varDecl(hasStaticStorageDuration(), unless(isStaticLocal()))
            .bind("staticvar"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    if (const auto* var = result.Nodes.getNodeAs<VarDecl>("staticvar")) {
      if (misra::libtooling_utils::IsInSystemHeader(var, result.Context))
        return;

      // Deside on initialization
      const Expr* init = var->getInit();
      // static member of a class may not use inline initialization
      // Therefore, check null pointer first
      if (!init) return;
      if (init->isCXX11ConstantExpr(*result.Context)) return;

      ReportError(
          misra::libtooling_utils::GetFilename(var, result.SourceManager),
          misra::libtooling_utils::GetLine(var, result.SourceManager),
          results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1166
}  // namespace googlecpp
