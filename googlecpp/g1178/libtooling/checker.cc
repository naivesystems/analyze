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

#include "googlecpp/g1178/libtooling/checker.h"

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
      "If possible, avoid defining operators as templates, because they must satisfy this rule for any possible template arguments";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1178 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    auto operators = hasAnyName(
        "operator+", "operator-", "operator*", "operator/", "operator%",
        "operator^", "operator&", "operator|", "operator~", "operator!",
        "operator=", "operator<", "operator>",
        "operator+=", "operator-=", "operator*=", "operator/=", "operator%=",
        "operator^=", "operator&=", "operator|=", "operator<<", "operator>>",
        "operator<<=", "operator>>=", "operator==", "operator!=", "operator<=",
        "operator>=", "operator&&", "operator||", "operator++", "operator--",
        "operator,", "operator->*", "operator->", "operator()", "operator[]");

    // for function template
    finder->addMatcher(
        functionTemplateDecl(unless(isExpansionInSystemHeader()), operators)
            .bind("op_decl"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    SourceManager* source_manager = result.SourceManager;
    auto* op = result.Nodes.getNodeAs<FunctionTemplateDecl>("op_decl");
    ReportError(misra::libtooling_utils::GetFilename(op, source_manager),
                misra::libtooling_utils::GetLine(op, source_manager),
                results_list_);
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1178
}  // namespace googlecpp
