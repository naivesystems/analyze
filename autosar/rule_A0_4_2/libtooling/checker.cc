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

#include "autosar/rule_A0_4_2/libtooling/checker.h"

#include <glog/logging.h>

#include <string>

#include "absl/strings/str_format.h"
#include "misra/libtooling_utils/libtooling_utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using analyzer::proto::ResultsList;

namespace {

void ReportError(string path, int line_number, ResultsList* results_list) {
  string error_message = "Type long double shall not be used.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace autosar {
namespace rule_A0_4_2 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        functionDecl(unless(isExpansionInSystemHeader())).bind("func"), this);
    finder->addMatcher(
        fieldDecl(unless(isExpansionInSystemHeader())).bind("field"), this);
    finder->addMatcher(varDecl(unless(isExpansionInSystemHeader())).bind("var"),
                       this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    auto checkTypeAndReport = [&result, this](const QualType type,
                                              const Decl* decl) {
      if (!type->isSpecificBuiltinType(BuiltinType::LongDouble)) return;
      ReportError(
          misra::libtooling_utils::GetFilename(decl, result.SourceManager),
          misra::libtooling_utils::GetLine(decl, result.SourceManager),
          results_list_);
    };

    if (const auto* func = result.Nodes.getNodeAs<FunctionDecl>("func")) {
      checkTypeAndReport(func->getReturnType(), func);
    } else if (const auto* field = result.Nodes.getNodeAs<FieldDecl>("field")) {
      checkTypeAndReport(field->getType(), field);
    } else if (const auto* var = result.Nodes.getNodeAs<VarDecl>("var")) {
      checkTypeAndReport(var->getType(), var);
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
}  // namespace rule_A0_4_2
}  // namespace autosar
