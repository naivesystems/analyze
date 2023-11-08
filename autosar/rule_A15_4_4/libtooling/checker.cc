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

#include "autosar/rule_A15_4_4/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace misra::proto_util;
using namespace clang::ast_matchers;

using analyzer::proto::ResultsList;
using std::string;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A declaration of non-throwing function shall contain noexcept specification.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A15_4_4 {
namespace libtooling {
// This checker matches the non-throwing function defitions which do not have
// proper noexcept specification.

// The function which meets one of the following conditions are regarded as a
// non-throwing function:
// (1) there exists a throw expression in the function body but is not handled
// by try...catch... .
// (2) call a function with the non-throwing noexcept specification.

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(
            isDefinition(),
            unless(anyOf(
                isNoThrow(),
                hasDescendant(cxxThrowExpr(unless(hasAncestor(cxxTryStmt())))),
                hasDescendant(callExpr(callee(functionDecl(isNoThrow())))))))
            .bind("decl"),
        this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const FunctionDecl* decl = result.Nodes.getNodeAs<FunctionDecl>("decl");
    if (decl) {
      const SourceLocation loc = decl->getLocation();
      if (!result.Context->getSourceManager().isInSystemHeader(loc)) {
        ReportError(
            misra::libtooling_utils::GetFilename(decl, result.SourceManager),
            misra::libtooling_utils::GetLine(decl, result.SourceManager),
            results_list_);
      }
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  results_list_ = result_list;
  callback_ = new Callback;
  callback_->Init(results_list_, &finder_);
}

}  // namespace libtooling
}  // namespace rule_A15_4_4
}  // namespace autosar
