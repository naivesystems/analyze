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

#include "googlecpp/g1152/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "clang/Lex/PreprocessingRecord.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message =
      "When a header declares inline functions or templates that clients of the header will instantiate, the inline functions and templates must also have definitions in the header, either directly or in files it includes.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1152 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(functionTemplateDecl().bind("template_func"), this);
    finder->addMatcher(functionDecl(isInline()).bind("func"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    auto checkIncludeAndReport = [&result, this](const FunctionDecl* func) {
      if (misra::libtooling_utils::IsInSystemHeader(func, result.Context))
        return;

      // Get the definition of the function
      const FunctionDecl* def = func->getDefinition();
      if (!def) return;
      SourceManager* sm = result.SourceManager;

      string filename = misra::libtooling_utils::GetFilename(func, sm);
      // We ignore it if the declaration is not in header.
      if (!misra::libtooling_utils::isInHeader(func, sm)) return;

      SourceLocation funcLoc = func->getLocation();
      SourceLocation defLoc = def->getLocation();
      // Declaration and definition in the same file
      if (sm->isWrittenInSameFile(funcLoc, defLoc)) return;

      FileID funcID = sm->getFileID(funcLoc);
      FileID defID = sm->getFileID(defLoc);
      // Definition is included in the declaration
      if (sm->getDecomposedIncludedLoc(defID).first == funcID) return;

      ReportError(filename, misra::libtooling_utils::GetLine(func, sm),
                  results_list_);
    };

    if (const auto* templateFunc =
            result.Nodes.getNodeAs<FunctionTemplateDecl>("template_func")) {
      if (templateFunc->specializations().empty()) return;
      if (const FunctionDecl* func = templateFunc->getAsFunction())
        checkIncludeAndReport(func);
    } else if (const auto* func =
                   result.Nodes.getNodeAs<FunctionDecl>("func")) {
      checkIncludeAndReport(func);
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
}  // namespace g1152
}  // namespace googlecpp
