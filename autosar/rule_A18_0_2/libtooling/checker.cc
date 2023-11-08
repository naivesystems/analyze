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

#include "autosar/rule_A18_0_2/libtooling/checker.h"

#include <glog/logging.h>

#include "absl/strings/str_format.h"
#include "clang/AST/QualTypeNames.h"
#include "misra/libtooling_utils/libtooling_utils.h"
using namespace misra::proto_util;
using namespace clang::ast_matchers;
using namespace clang;

using analyzer::proto::ResultsList;
using std::string;

static bool isUsingStd = false;

namespace {

void ReportError(const std::string& path, int line_number,
                 ResultsList* results_list) {
  std::string error_message =
      "The error state of a conversion from string to a numeric value shall be checked.";
  misra::proto_util::AddResultToResultsList(results_list, path, line_number,
                                            error_message);
  LOG(INFO) << absl::StrFormat("%s, path: %s, line: %d", error_message, path,
                               line_number);
}

}  // namespace

// This checker tries to match the covertion from a string to a number by using
// operator >> and stream:
// std::istream cin;
// std::stringstream ss;
// int num;
// cin >> num; ss >> num;

// cin >> num; and ss >> num; will be matched. And in the following two cases,
// the usage is valid:
// (1) the >> operator is used in a try block
// (2) cin.fail() or ss.fail() is called atfer >> is found.

namespace autosar {
namespace rule_A18_0_2 {
namespace libtooling {

class Callback : public MatchFinder::MatchCallback {
 public:
  void Init(analyzer::proto::ResultsList* results_list,
            ast_matchers::MatchFinder* finder) {
    results_list_ = results_list;
    clang::ast_matchers::internal::BindableMatcher<clang::Stmt> dre =
        declRefExpr(to(varDecl(anyOf(hasType(asString("std::istream")),
                                     hasType(asString("std::stringstream"))))));
    finder->addMatcher(
        cxxOperatorCallExpr(
            hasOverloadedOperatorName(">>"),
            hasLHS(anyOf(dre, hasDescendant(dre))),
            hasRHS(declRefExpr(to(varDecl(anyOf(
                hasType(isInteger()), hasType(realFloatingPointType())))))),
            unless(anyOf(hasAncestor(cxxTryStmt()), isExpansionInSystemHeader(),
                         hasParent(stmt(hasDescendant(cxxMemberCallExpr(
                             callee(functionDecl(hasName("fail"))))))))))
            .bind("ce"),
        this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult& result) {
    const CXXOperatorCallExpr* ce =
        result.Nodes.getNodeAs<CXXOperatorCallExpr>("ce");
    std::string path =
        misra::libtooling_utils::GetFilename(ce, result.SourceManager);
    int line_number =
        misra::libtooling_utils::GetLine(ce, result.SourceManager);
    ReportError(path, line_number, results_list_);
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
}  // namespace rule_A18_0_2
}  // namespace autosar
