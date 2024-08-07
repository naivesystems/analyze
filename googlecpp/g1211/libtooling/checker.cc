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

#include "googlecpp/g1211/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using std::string;
using namespace clang;
using namespace misra::proto_util;

namespace {

void ReportError(string path, int line_number,
                 analyzer::proto::ResultsList* results_list) {
  string error_message = "Don't introduce new names in captures";
  AddResultToResultsList(results_list, path, line_number, error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1211 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;

    finder->addMatcher(
        lambdaExpr(unless(isExpansionInSystemHeader())).bind("lambda"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    const LambdaExpr* lambda = result.Nodes.getNodeAs<LambdaExpr>("lambda");

    for (clang::LambdaExpr::capture_iterator it = lambda->capture_begin();
         it != lambda->capture_end(); ++it) {
      if (lambda->isInitCapture(it)) {
        ReportError(
            misra::libtooling_utils::GetFilename(lambda, result.SourceManager),
            misra::libtooling_utils::GetLine(lambda, result.SourceManager),
            results_list_);
        return;
      }
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
}  // namespace g1211
}  // namespace googlecpp
