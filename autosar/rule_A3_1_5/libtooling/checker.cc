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

#include "autosar/rule_A3_1_5/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/StmtVisitor.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace ast_matchers;

namespace {

constexpr char kFuncDeclString[] = "functionDecl";

struct FuncDeclInfo {
  std::string path;
  int line_number;
};

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "A function definition shall only be placed in a class definition if (1) the function is intended to be inlined (2) it is a member function template (3) it is a member function of a class template.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}
}  // namespace

namespace autosar {
namespace rule_A3_1_5 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(ResultsList* results_list, MatchFinder* finder);
  void run(const MatchFinder::MatchResult& result) override;

 private:
  ResultsList* results_list_;
};

void Callback::Init(ResultsList* results_list, MatchFinder* finder) {
  results_list_ = results_list;
  finder->addMatcher(
      cxxMethodDecl(unless(isExpansionInSystemHeader()), unless(isDefinition()))
          .bind(kFuncDeclString),
      this);
}

void Callback::run(const MatchFinder::MatchResult& result) {
  const auto* method = result.Nodes.getNodeAs<CXXMethodDecl>(kFuncDeclString);
  if (const auto* cls = method->getParent()) {
    if (cls->getDescribedClassTemplate()) {
      std::string path =
          misra::libtooling_utils::GetFilename(method, result.SourceManager);
      int line_number =
          misra::libtooling_utils::GetLine(method, result.SourceManager);
      ReportError(path, line_number, results_list_);
    }
  }

  if (method->getDescribedFunctionTemplate() || method->isInlined() ||
      method->isConst()) {
    std::string path =
        misra::libtooling_utils::GetFilename(method, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(method, result.SourceManager);
    ReportError(path, line_number, results_list_);
  }
}

void Checker::Init(ResultsList* results_list) {
  results_list_ = results_list;
  callback_ = new Callback;
  callback_->Init(results_list, &finder_);
}

MatchFinder* Checker::GetMatchFinder() { return &finder_; }

}  // namespace libtooling
}  // namespace rule_A3_1_5
}  // namespace autosar
