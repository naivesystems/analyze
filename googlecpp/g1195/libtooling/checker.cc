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

#include "googlecpp/g1195/libtooling/checker.h"

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
      "Use the prefix form (++i) of the increment and decrement operators unless you need postfix semantics";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

namespace googlecpp {
namespace g1195 {
namespace libtooling {

class Callback : public ast_matchers::MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    finder->addMatcher(
        unaryOperator(
            unless(anyOf(isExpansionInSystemHeader(), hasParent(varDecl()),
                         hasAncestor(varDecl()),  // example: int a = data[i++];
                         hasAncestor(binaryOperation(hasOperatorName("="))))))
            .bind("uo1"),
        this);
    finder->addMatcher(
        cxxOperatorCallExpr(
            argumentCountIs(2),
            unless(anyOf(isExpansionInSystemHeader(), hasAncestor(varDecl()),
                         hasAncestor(varDecl()),
                         binaryOperation(hasOperatorName("=")))))
            .bind("uo2"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) override {
    ASTContext* context = result.Context;
    SourceManager* source_manager = result.SourceManager;
    auto* uo1 = result.Nodes.getNodeAs<UnaryOperator>("uo1");
    auto* uo2 = result.Nodes.getNodeAs<CXXOperatorCallExpr>("uo2");
    if (uo1) {
      if (!uo1->isPostfix()) {
        return;
      }

      ReportError(misra::libtooling_utils::GetFilename(uo1, source_manager),
                  misra::libtooling_utils::GetLine(uo1, source_manager),
                  results_list_);
    } else {
      const OverloadedOperatorKind opKind = uo2->getOperator();
      if (opKind != OO_PlusPlus && opKind != OO_MinusMinus) {
        return;
      }
      ReportError(misra::libtooling_utils::GetFilename(uo2, source_manager),
                  misra::libtooling_utils::GetLine(uo2, source_manager),
                  results_list_);
    }
  }

 private:
  analyzer::proto::ResultsList* results_list_;
};

void Checker::Init(analyzer::proto::ResultsList* result_list) {
  callback_ = new Callback;
  callback_->Init(result_list, &finder_);
}
}  // namespace libtooling
}  // namespace g1195
}  // namespace googlecpp
